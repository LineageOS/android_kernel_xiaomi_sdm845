/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * DOC: wlan_serialization_enqueue.c
 * This file defines the routines which are pertinent
 * to the queuing of commands.
 */
#include <wlan_serialization_api.h>
#include "wlan_serialization_main_i.h"
#include "wlan_serialization_utils_i.h"
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <qdf_list.h>

static enum wlan_serialization_status
wlan_serialization_add_cmd_to_given_queue(qdf_list_t *queue,
			struct wlan_serialization_command *cmd,
			struct wlan_objmgr_psoc *psoc,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj,
			uint8_t is_cmd_for_active_queue,
			struct wlan_serialization_command_list **pcmd_list)
{
	struct wlan_serialization_command_list *cmd_list;
	enum wlan_serialization_status status;
	QDF_STATUS qdf_status;
	qdf_list_node_t *nnode;

	if (!cmd || !queue || !ser_pdev_obj || !psoc) {
		serialization_err("Input arguments are not valid");
		return WLAN_SER_CMD_DENIED_UNSPECIFIED;
	}

	if ((cmd->cmd_type < WLAN_SER_CMD_NONSCAN) &&
	    !wlan_serialization_is_scan_cmd_allowed(psoc, ser_pdev_obj)) {
		serialization_err("Failed to add scan cmd id %d type %d, Scan cmd list is full",
				  cmd->cmd_id, cmd->cmd_type);
		return WLAN_SER_CMD_DENIED_LIST_FULL;
	}
	if (wlan_serialization_list_empty(&ser_pdev_obj->global_cmd_pool_list,
					  ser_pdev_obj)) {
		serialization_err("Failed to add cmd id %d type %d, Cmd list is full",
				  cmd->cmd_id, cmd->cmd_type);
		return WLAN_SER_CMD_DENIED_LIST_FULL;
	}
	if (wlan_serialization_remove_front(&ser_pdev_obj->global_cmd_pool_list,
					    &nnode, ser_pdev_obj) !=
					    QDF_STATUS_SUCCESS) {
		serialization_err("Failed to get cmd buffer from pool for cmd id %d type %d",
				  cmd->cmd_id, cmd->cmd_type);
		return WLAN_SER_CMD_DENIED_UNSPECIFIED;
	}
	cmd_list = qdf_container_of(nnode,
			struct wlan_serialization_command_list, node);
	qdf_mem_copy(&cmd_list->cmd, cmd,
			sizeof(struct wlan_serialization_command));
	if (cmd->is_high_priority)
		qdf_status = wlan_serialization_insert_front(queue,
							     &cmd_list->node,
							     ser_pdev_obj);
	else
		qdf_status = wlan_serialization_insert_back(queue,
							    &cmd_list->node,
							    ser_pdev_obj);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		qdf_mem_zero(&cmd_list->cmd,
				sizeof(struct wlan_serialization_command));
		qdf_status = wlan_serialization_insert_back(
					&ser_pdev_obj->global_cmd_pool_list,
					&cmd_list->node,
					ser_pdev_obj);
		if (QDF_STATUS_SUCCESS != qdf_status) {
			serialization_err("can't put cmd back to global pool");
			QDF_ASSERT(0);
		}
		return WLAN_SER_CMD_DENIED_UNSPECIFIED;
	}
	qdf_atomic_set_bit(CMD_IS_ACTIVE, &cmd_list->cmd_in_use);
	*pcmd_list = cmd_list;
	if (is_cmd_for_active_queue)
		status = WLAN_SER_CMD_ACTIVE;
	else
		status = WLAN_SER_CMD_PENDING;

	return status;
}

void wlan_serialization_activate_cmd(
			struct wlan_serialization_command_list *cmd_list,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	qdf_list_t *queue = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	if (!cmd_list) {
		serialization_err("invalid cmd_list");
		QDF_ASSERT(0);
		return;
	}
	if (cmd_list->cmd.cmd_type < WLAN_SER_CMD_NONSCAN)
		queue = &ser_pdev_obj->active_scan_list;
	else
		queue = &ser_pdev_obj->active_list;
	if (wlan_serialization_list_empty(queue, ser_pdev_obj)) {
		serialization_err("nothing in active queue");
		QDF_ASSERT(0);
		return;
	}
	if (!cmd_list->cmd.cmd_cb) {
		serialization_err("no cmd_cb for cmd type:%d, id: %d",
			cmd_list->cmd.cmd_type,
			cmd_list->cmd.cmd_id);
		QDF_ASSERT(0);
		return;
	}

	if (cmd_list->cmd.vdev) {
		psoc = wlan_vdev_get_psoc(cmd_list->cmd.vdev);
		if (psoc == NULL) {
			serialization_err("invalid psoc");
			return;
		}
	} else {
		serialization_err("invalid cmd.vdev");
		return;
	}
	/*
	 * command is already pushed to active queue above
	 * now start the timer and notify requestor
	 */
	wlan_serialization_find_and_start_timer(psoc,
						&cmd_list->cmd);

	serialization_debug("Activate type %d id %d", cmd_list->cmd.cmd_type,
			    cmd_list->cmd.cmd_id);
	/*
	 * Remember that serialization module may send
	 * this callback in same context through which it
	 * received the serialization request. Due to which
	 * it is caller's responsibility to ensure acquiring
	 * and releasing its own lock appropriately.
	 */
	qdf_status = cmd_list->cmd.cmd_cb(&cmd_list->cmd,
				WLAN_SER_CB_ACTIVATE_CMD);
	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		return;
	/*
	 * Since the command activation has not succeeded,
	 * remove the cmd from the active list and before
	 * doing so, try to mark the cmd for delete so that
	 * it is not accessed in other thread context for deletion
	 * again.
	 */
	if (wlan_serialization_is_cmd_present_in_active_queue(
		psoc, &cmd_list->cmd)) {
		wlan_serialization_find_and_stop_timer(psoc,
						&cmd_list->cmd);
		if (qdf_atomic_test_and_set_bit(CMD_MARKED_FOR_DELETE,
						&cmd_list->cmd_in_use)) {
			serialization_debug("SER_CMD already being deleted");
		} else {
			serialization_debug("SER_CMD marked for removal");
			cmd_list->cmd.cmd_cb(&cmd_list->cmd,
					     WLAN_SER_CB_RELEASE_MEM_CMD);
			wlan_serialization_put_back_to_global_list(queue,
								   ser_pdev_obj,
								   cmd_list);
		}
	} else {
		serialization_err("active cmd :%d,id:%d is removed already",
				  cmd_list->cmd.cmd_type,
				  cmd_list->cmd.cmd_id);
	}
	wlan_serialization_move_pending_to_active(
				cmd_list->cmd.cmd_type,
				ser_pdev_obj);
}

enum wlan_serialization_status
wlan_serialization_enqueue_cmd(struct wlan_serialization_command *cmd,
			uint8_t is_cmd_for_active_queue,
			struct wlan_serialization_command_list **pcmd_list)
{
	enum wlan_serialization_status status = WLAN_SER_CMD_DENIED_UNSPECIFIED;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj;
	qdf_list_t *queue;

	/* Enqueue process
	 * 1) peek through command structure and see what is the command type
	 * 2) two main types of commands to process
	 *    a) SCAN
	 *    b) NON-SCAN
	 * 3) for each command there are separate command queues per pdev
	 * 4) pull pdev from vdev structure and get the command queue associated
	 *    with that pdev and try to enqueue on those queue
	 * 5) Thumb rule:
	 *    a) There could be only 1 active non-scan command at a
	 *       time including all total non-scan commands of all pdevs.
	 *
	 *       example: pdev1 has 1 non-scan active command and
	 *       pdev2 got 1 non-scan command then that command should go to
	 *       pdev2's pending queue
	 *
	 *    b) There could be only N number of scan commands at a time
	 *       including all total scan commands of all pdevs
	 *
	 *       example: Let's say N=8,
	 *       pdev1's vdev1 has 5 scan command, pdev2's vdev1 has 3
	 *       scan commands, if we get scan request on vdev2 then it will go
	 *       to pending queue of vdev2 as we reached max allowed scan active
	 *       command.
	 */
	if (!cmd) {
		serialization_err("NULL command");
		return status;
	}
	if (!cmd->cmd_cb) {
		serialization_err("no cmd_cb for cmd type:%d, id: %d",
			cmd->cmd_type,
			cmd->cmd_id);
		return status;
	}
	pdev = wlan_serialization_get_pdev_from_cmd(cmd);
	if (pdev == NULL) {
		serialization_err("invalid pdev");
		return status;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (psoc == NULL) {
		serialization_err("invalid psoc");
		return status;
	}

	/* get priv object by wlan_objmgr_vdev_get_comp_private_obj */
	ser_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_SERIALIZATION);
	if (!ser_pdev_obj) {
		serialization_err("Can't find ser_pdev_obj");
		return status;
	}

	serialization_debug("Type %d id %d high_priority %d timeout %d for active %d",
			    cmd->cmd_type, cmd->cmd_id, cmd->is_high_priority,
			    cmd->cmd_timeout_duration, is_cmd_for_active_queue);

	if (cmd->cmd_type < WLAN_SER_CMD_NONSCAN) {
		if (is_cmd_for_active_queue)
			queue = &ser_pdev_obj->active_scan_list;
		else
			queue = &ser_pdev_obj->pending_scan_list;
	} else {
		if (is_cmd_for_active_queue)
			queue = &ser_pdev_obj->active_list;
		else
			queue = &ser_pdev_obj->pending_list;
	}

	if (wlan_serialization_is_cmd_present_queue(cmd,
				is_cmd_for_active_queue)) {
		serialization_err("duplicate command Type %d id %d, can't enqueue",
				  cmd->cmd_type, cmd->cmd_id);
		return status;
	}

	return wlan_serialization_add_cmd_to_given_queue(queue, cmd, psoc,
			ser_pdev_obj, is_cmd_for_active_queue, pcmd_list);
}
