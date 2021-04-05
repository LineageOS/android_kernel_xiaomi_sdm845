/*
 *  linux/fs/ext4/block_validity.c
 *
 * Copyright (C) 2009
 * Theodore Ts'o (tytso@mit.edu)
 *
 * Track which blocks in the filesystem are metadata blocks that
 * should never be used as data blocks by files or directories.
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/quotaops.h>
#include <linux/buffer_head.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include "ext4.h"

struct ext4_system_zone {
	struct rb_node	node;
	ext4_fsblk_t	start_blk;
	unsigned int	count;
	u32		ino;
};

static struct kmem_cache *ext4_system_zone_cachep;

int __init ext4_init_system_zone(void)
{
	ext4_system_zone_cachep = KMEM_CACHE(ext4_system_zone, 0);
	if (ext4_system_zone_cachep == NULL)
		return -ENOMEM;
	return 0;
}

void ext4_exit_system_zone(void)
{
	kmem_cache_destroy(ext4_system_zone_cachep);
}

static inline int can_merge(struct ext4_system_zone *entry1,
		     struct ext4_system_zone *entry2)
{
	if ((entry1->start_blk + entry1->count) == entry2->start_blk &&
	    entry1->ino == entry2->ino)
		return 1;
	return 0;
}

/*
 * Mark a range of blocks as belonging to the "system zone" --- that
 * is, filesystem metadata blocks which should never be used by
 * inodes.
 */
static int add_system_zone(struct ext4_sb_info *sbi,
			   ext4_fsblk_t start_blk,
			   unsigned int count, u32 ino)
{
	struct ext4_system_zone *new_entry, *entry;
	struct rb_node **n = &sbi->system_blks.rb_node, *node;
	struct rb_node *parent = NULL, *new_node = NULL;

	while (*n) {
		parent = *n;
		entry = rb_entry(parent, struct ext4_system_zone, node);
		if (start_blk < entry->start_blk)
			n = &(*n)->rb_left;
		else if (start_blk >= (entry->start_blk + entry->count))
			n = &(*n)->rb_right;
		else	/* Unexpected overlap of system zones. */
			return -EFSCORRUPTED;
	}

	new_entry = kmem_cache_alloc(ext4_system_zone_cachep,
				     GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;
	new_entry->start_blk = start_blk;
	new_entry->count = count;
	new_entry->ino = ino;
	new_node = &new_entry->node;

	rb_link_node(new_node, parent, n);
	rb_insert_color(new_node, &sbi->system_blks);

	/* Can we merge to the left? */
	node = rb_prev(new_node);
	if (node) {
		entry = rb_entry(node, struct ext4_system_zone, node);
		if (can_merge(entry, new_entry)) {
			new_entry->start_blk = entry->start_blk;
			new_entry->count += entry->count;
			rb_erase(node, &sbi->system_blks);
			kmem_cache_free(ext4_system_zone_cachep, entry);
		}
	}

	/* Can we merge to the right? */
	node = rb_next(new_node);
	if (node) {
		entry = rb_entry(node, struct ext4_system_zone, node);
		if (can_merge(new_entry, entry)) {
			new_entry->count += entry->count;
			rb_erase(node, &sbi->system_blks);
			kmem_cache_free(ext4_system_zone_cachep, entry);
		}
	}
	return 0;
}

static void debug_print_tree(struct ext4_sb_info *sbi)
{
	struct rb_node *node;
	struct ext4_system_zone *entry;
	int first = 1;

	printk(KERN_INFO "System zones: ");
	node = rb_first(&sbi->system_blks);
	while (node) {
		entry = rb_entry(node, struct ext4_system_zone, node);
		printk(KERN_CONT "%s%llu-%llu", first ? "" : ", ",
		       entry->start_blk, entry->start_blk + entry->count - 1);
		first = 0;
		node = rb_next(node);
	}
	printk(KERN_CONT "\n");
}

static int ext4_protect_reserved_inode(struct super_block *sb, u32 ino)
{
	struct inode *inode;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_map_blocks map;
	u32 i = 0, num;
	int err = 0, n;

	if ((ino < EXT4_ROOT_INO) ||
	    (ino > le32_to_cpu(sbi->s_es->s_inodes_count)))
		return -EINVAL;
	inode = ext4_iget(sb, ino, EXT4_IGET_SPECIAL);
	if (IS_ERR(inode))
		return PTR_ERR(inode);
	num = (inode->i_size + sb->s_blocksize - 1) >> sb->s_blocksize_bits;
	while (i < num) {
		cond_resched();
		map.m_lblk = i;
		map.m_len = num - i;
		n = ext4_map_blocks(NULL, inode, &map, 0);
		if (n < 0) {
			err = n;
			break;
		}
		if (n == 0) {
			i++;
		} else {
			err = add_system_zone(sbi, map.m_pblk, n, ino);
			if (err < 0) {
				if (err == -EFSCORRUPTED) {
					ext4_error(sb,
					   "blocks %llu-%llu from inode %u "
					   "overlap system zone", map.m_pblk,
					   map.m_pblk + map.m_len - 1, ino);
				}
				break;
			}
			i += n;
		}
	}
	iput(inode);
	return err;
}

int ext4_setup_system_zone(struct super_block *sb)
{
	ext4_group_t ngroups = ext4_get_groups_count(sb);
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_group_desc *gdp;
	ext4_group_t i;
	int flex_size = ext4_flex_bg_size(sbi);
	int ret;

	if (!test_opt(sb, BLOCK_VALIDITY)) {
		if (EXT4_SB(sb)->system_blks.rb_node)
			ext4_release_system_zone(sb);
		return 0;
	}
	if (EXT4_SB(sb)->system_blks.rb_node)
		return 0;

	for (i=0; i < ngroups; i++) {
		if (ext4_bg_has_super(sb, i) &&
		    ((i < 5) || ((i % flex_size) == 0)))
			add_system_zone(sbi, ext4_group_first_block_no(sb, i),
					ext4_bg_num_gdb(sb, i) + 1, 0);
		gdp = ext4_get_group_desc(sb, i, NULL);
		ret = add_system_zone(sbi, ext4_block_bitmap(sb, gdp), 1, 0);
		if (ret)
			return ret;
		ret = add_system_zone(sbi, ext4_inode_bitmap(sb, gdp), 1, 0);
		if (ret)
			return ret;
		ret = add_system_zone(sbi, ext4_inode_table(sb, gdp),
				sbi->s_itb_per_group, 0);
		if (ret)
			return ret;
	}
	if (ext4_has_feature_journal(sb) && sbi->s_es->s_journal_inum) {
		ret = ext4_protect_reserved_inode(sb,
				le32_to_cpu(sbi->s_es->s_journal_inum));
		if (ret)
			return ret;
	}

	if (test_opt(sb, DEBUG))
		debug_print_tree(EXT4_SB(sb));
	return 0;
}

/* Called when the filesystem is unmounted */
void ext4_release_system_zone(struct super_block *sb)
{
	struct ext4_system_zone	*entry, *n;

	rbtree_postorder_for_each_entry_safe(entry, n,
			&EXT4_SB(sb)->system_blks, node)
		kmem_cache_free(ext4_system_zone_cachep, entry);

	EXT4_SB(sb)->system_blks = RB_ROOT;
}

/*
 * Returns 1 if the passed-in block region (start_blk,
 * start_blk+count) is valid; 0 if some part of the block region
 * overlaps with filesystem metadata blocks.
 */
int ext4_inode_block_valid(struct inode *inode, ext4_fsblk_t start_blk,
			   unsigned int count)
{
	struct ext4_system_zone *entry;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct rb_node *n = sbi->system_blks.rb_node;

	if ((start_blk <= le32_to_cpu(sbi->s_es->s_first_data_block)) ||
	    (start_blk + count < start_blk) ||
	    (start_blk + count > ext4_blocks_count(sbi->s_es))) {
		sbi->s_es->s_last_error_block = cpu_to_le64(start_blk);
		return 0;
	}
	while (n) {
		entry = rb_entry(n, struct ext4_system_zone, node);
		if (start_blk + count - 1 < entry->start_blk)
			n = n->rb_left;
		else if (start_blk >= (entry->start_blk + entry->count))
			n = n->rb_right;
		else {
			if (entry->ino == inode->i_ino)
				return 1;
			sbi->s_es->s_last_error_block = cpu_to_le64(start_blk);
			return 0;
		}
	}
	return 1;
}

int ext4_check_blockref(const char *function, unsigned int line,
			struct inode *inode, __le32 *p, unsigned int max)
{
	struct ext4_super_block *es = EXT4_SB(inode->i_sb)->s_es;
	__le32 *bref = p;
	unsigned int blk;

	if (ext4_has_feature_journal(inode->i_sb) &&
	    (inode->i_ino ==
	     le32_to_cpu(EXT4_SB(inode->i_sb)->s_es->s_journal_inum)))
		return 0;

	while (bref < p+max) {
		blk = le32_to_cpu(*bref++);
		if (blk &&
		    unlikely(!ext4_inode_block_valid(inode, blk, 1))) {
			es->s_last_error_block = cpu_to_le64(blk);
			ext4_error_inode(inode, function, line, blk,
					 "invalid block");
			return -EFSCORRUPTED;
		}
	}
	return 0;
}

