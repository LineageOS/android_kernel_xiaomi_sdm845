/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_serialization_utils.c
 * This file defines the utility helper functions for serialization component.
 */

#include "wlan_serialization_utils_i.h"
#include "wlan_serialization_main_i.h"
#include "wlan_serialization_api.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "qdf_mc_timer.h"
#include "wlan_utility.h"
#include "wlan_scan_ucfg_api.h"
#ifdef CONFIG_MCL
#include "qdf_platform.h"
#endif

QDF_STATUS
wlan_serialization_put_back_to_global_list(qdf_list_t *queue,
		struct wlan_serialization_pdev_priv_obj *ser_pdev_obj,
		struct wlan_serialization_command_list *cmd_list)
{
	QDF_STATUS status;
	uint32_t cmd_id, cmd_type;

	if (!queue || !ser_pdev_obj || !cmd_list) {
		serialization_err("input parameters are invalid");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * if the command is already removed in other context,
	 * then it will be marked as inactive with the same
	 * below code. So, test before proceeding.
	 */
	if (!qdf_atomic_test_and_clear_bit(CMD_IS_ACTIVE,
					   &cmd_list->cmd_in_use)) {
		serialization_debug("CMD is not active or being used");
		return QDF_STATUS_SUCCESS;
	}
	status = wlan_serialization_remove_node(queue, &cmd_list->node,
						ser_pdev_obj);
	if (QDF_STATUS_SUCCESS != status) {
		serialization_err("can't remove cmd from queue");
		/* assert to catch any leaks */
		QDF_ASSERT(0);
		return status;
	}
	cmd_id = cmd_list->cmd.cmd_id;
	cmd_type = cmd_list->cmd.cmd_type;
	qdf_mem_zero(&cmd_list->cmd, sizeof(struct wlan_serialization_command));
	status = wlan_serialization_insert_back(
			&ser_pdev_obj->global_cmd_pool_list,
			&cmd_list->node, ser_pdev_obj);
	qdf_atomic_clear_bit(CMD_MARKED_FOR_DELETE, &cmd_list->cmd_in_use);
	if (QDF_STATUS_SUCCESS != status) {
		serialization_err("can't put command back to global pool");
		QDF_ASSERT(0);
	}
	serialization_debug("cmd_id-%d, cmd_type-%d", cmd_id, cmd_type);

	return status;
}

struct wlan_objmgr_pdev*
wlan_serialization_get_pdev_from_cmd(struct wlan_serialization_command *cmd)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!cmd) {
		serialization_err("invalid cmd");
		return pdev;
	}
	if (!cmd->vdev) {
		serialization_err("invalid cmd->vdev");
		return pdev;
	}
	pdev = wlan_vdev_get_pdev(cmd->vdev);

	return pdev;
}

QDF_STATUS wlan_serialization_get_cmd_from_queue(qdf_list_t *queue,
			qdf_list_node_t **nnode,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;
	qdf_list_node_t *pnode;

	if (!queue || !ser_pdev_obj) {
		serialization_err("input parameters are invalid");
		return QDF_STATUS_E_FAILURE;
	}

	pnode = *nnode;
	if (!pnode)
		status = wlan_serialization_peek_front(queue, nnode,
						       ser_pdev_obj);
	else
		status = wlan_serialization_peek_next(queue, pnode, nnode,
						      ser_pdev_obj);

	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("can't get next node from queue");
	}

	return status;
}

/**
 * wlan_serialization_timer_destroy() - destroys the timer
 * @ser_timer: pointer to particular timer
 *
 * This API destroys the memory allocated by timer and assigns cmd member of
 * that timer structure to NULL
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_serialization_timer_destroy(
		struct wlan_serialization_timer *ser_timer)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!ser_timer || !ser_timer->cmd) {
		serialization_debug("Invalid ser_timer");
		return status;
	}
	status = qdf_mc_timer_destroy(&ser_timer->timer);
	if (QDF_IS_STATUS_ERROR(status)) {
		serialization_err("Failed to destroy timer for cmd_id[%d]",
				ser_timer->cmd->cmd_id);
		return status;
	}
	ser_timer->cmd = NULL;

	return status;
}

#ifdef CONFIG_MCL
static void wlan_serialization_non_scan_timeout_action(void)
{
	qdf_trigger_self_recovery();
}
#else
static void wlan_serialization_non_scan_timeout_action(void)
{
	QDF_BUG(0);
}
#endif

/**
 * wlan_serialization_generic_timer_callback() - timer callback when timer fire
 * @arg: argument that timer passes to this callback
 *
 * All the timers in serialization module calls this callback when they fire,
 * and this API in turn calls command specific timeout callback and remove
 * timed-out command from active queue and move any pending command to active
 * queue of same cmd_type.
 *
 * Return: none
 */
static void wlan_serialization_generic_timer_callback(void *arg)
{
	struct wlan_serialization_timer *timer = arg;
	struct wlan_serialization_command *cmd = timer->cmd;
	uint8_t vdev_id = WLAN_INVALID_VDEV_ID;

	if (!cmd) {
		serialization_err("command not found");
		QDF_ASSERT(0);
		return;
	}

	if (cmd->vdev)
		vdev_id = wlan_vdev_get_id(cmd->vdev);

	serialization_err("active cmd timeout for cmd_type[%d] vdev_id[%d]",
			  cmd->cmd_type, vdev_id);

	if (cmd->cmd_cb)
		cmd->cmd_cb(cmd, WLAN_SER_CB_ACTIVE_CMD_TIMEOUT);

	if (cmd->cmd_type >= WLAN_SER_CMD_NONSCAN)
		wlan_serialization_non_scan_timeout_action();
	/*
	 * dequeue cmd API will cleanup and destroy the timer. If it fails to
	 * dequeue command then we have to destroy the timer. It will also call
	 * cmd callback with WLAN_SER_CB_RELEASE_MEM_CMD to free the memory.
	 */
	if (WLAN_SER_CMD_NOT_FOUND == wlan_serialization_dequeue_cmd(cmd, true))
		wlan_serialization_timer_destroy(timer);
	if (cmd->cmd_cb)
		cmd->cmd_cb(cmd, WLAN_SER_CB_RELEASE_MEM_CMD);
}

/**
 * wlan_serialization_stop_timer() - to stop particular timer
 * @ser_timer: pointer to serialization timer
 *
 * This API stops the particular timer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_serialization_stop_timer(struct wlan_serialization_timer *ser_timer)
{
	QDF_TIMER_STATE state;
	QDF_STATUS status;

	state = qdf_mc_timer_get_current_state(&ser_timer->timer);
	if (QDF_TIMER_STATE_RUNNING != state &&
			QDF_TIMER_STATE_STARTING != state) {
		serialization_debug("nothing to stop");
		wlan_serialization_timer_destroy(ser_timer);
		return QDF_STATUS_SUCCESS;
	}
	status = qdf_mc_timer_stop(&ser_timer->timer);
	if (QDF_IS_STATUS_ERROR(status)) {
		serialization_err("Failed to stop timer");
		return status;
	}
	wlan_serialization_timer_destroy(ser_timer);
	status = QDF_STATUS_SUCCESS;

	return status;
}

QDF_STATUS wlan_serialization_cleanup_all_timers(
			struct wlan_serialization_psoc_priv_obj *psoc_ser_obj)
{
	struct wlan_serialization_timer *ser_timer;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i = 0;

	if (!psoc_ser_obj) {
		serialization_err("Invalid psoc_ser_obj");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_serialization_acquire_lock(&psoc_ser_obj->timer_lock);
	for (i = 0; psoc_ser_obj->max_active_cmds > i; i++) {
		ser_timer = &psoc_ser_obj->timers[i];
		if (!ser_timer->cmd)
			continue;
		status = wlan_serialization_stop_timer(ser_timer);
		if (QDF_STATUS_SUCCESS != status) {
			/* lets not break the loop but report error */
			serialization_err("some error in stopping timer");
		}
	}
	wlan_serialization_release_lock(&psoc_ser_obj->timer_lock);

	return status;
}

QDF_STATUS
wlan_serialization_find_and_stop_timer(struct wlan_objmgr_psoc *psoc,
				       struct wlan_serialization_command *cmd)
{
	struct wlan_serialization_psoc_priv_obj *psoc_ser_obj;
	struct wlan_serialization_timer *ser_timer;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int i = 0;

	if (!psoc || !cmd) {
		serialization_err("invalid param");
		return status;
	}

	if ((cmd->cmd_timeout_duration == 0) &&
		(wlan_is_emulation_platform(wlan_psoc_get_nif_phy_version(psoc)
	))) {
		serialization_err("[SCAN-EMULATION]: Not performing timer functions\n");
		return QDF_STATUS_SUCCESS;
	}

	psoc_ser_obj = wlan_serialization_get_psoc_priv_obj(psoc);
	wlan_serialization_acquire_lock(&psoc_ser_obj->timer_lock);
	/*
	 * Here cmd_id and cmd_type are used to locate the timer being
	 * associated with command. For scan command, cmd_id is expected to
	 * be unique and For non-scan command, there should be only one active
	 * command per pdev
	 */
	for (i = 0; psoc_ser_obj->max_active_cmds > i; i++) {
		ser_timer = &psoc_ser_obj->timers[i];
		if (!(ser_timer->cmd) ||
				(ser_timer->cmd->cmd_id != cmd->cmd_id) ||
				(ser_timer->cmd->cmd_type != cmd->cmd_type) ||
				(ser_timer->cmd->vdev != cmd->vdev))
			continue;
		status = wlan_serialization_stop_timer(ser_timer);
		status = QDF_STATUS_SUCCESS;
		break;
	}
	wlan_serialization_release_lock(&psoc_ser_obj->timer_lock);

	if (QDF_IS_STATUS_SUCCESS(status))
		serialization_debug("Stopped timer for cmd_type %d cmd id %d",
				    cmd->cmd_type, cmd->cmd_id);
	else
		serialization_err("can't find timer for cmd_type %d cmd_id %d",
				  cmd->cmd_type, cmd->cmd_id);
	return status;
}

QDF_STATUS
wlan_serialization_find_and_start_timer(struct wlan_objmgr_psoc *psoc,
					struct wlan_serialization_command *cmd)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_serialization_psoc_priv_obj *psoc_ser_obj;
	struct wlan_serialization_timer *ser_timer;
	int i = 0;

	if (!psoc || !cmd) {
		serialization_err("invalid param");
		return status;
	}

	if ((cmd->cmd_timeout_duration == 0) &&
		(wlan_is_emulation_platform(wlan_psoc_get_nif_phy_version(psoc)
	))) {
		serialization_err("[SCAN-EMULATION]: Not performing timer functions\n");
		return QDF_STATUS_SUCCESS;
	}


	psoc_ser_obj = wlan_serialization_get_psoc_priv_obj(psoc);

	wlan_serialization_acquire_lock(&psoc_ser_obj->timer_lock);
	for (i = 0; psoc_ser_obj->max_active_cmds > i; i++) {
		/* Keep trying timer */
		ser_timer = &psoc_ser_obj->timers[i];
		if (ser_timer->cmd)
			continue;
		/* Remember timer is pointing to command */
		ser_timer->cmd = cmd;
		status = QDF_STATUS_SUCCESS;
		break;
	}
	wlan_serialization_release_lock(&psoc_ser_obj->timer_lock);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = qdf_mc_timer_init(&ser_timer->timer, QDF_TIMER_TYPE_SW,
				   wlan_serialization_generic_timer_callback,
				   ser_timer);
		if (QDF_IS_STATUS_ERROR(status)) {
			serialization_err("Failed to init timer cmdid [%d]",
					cmd->cmd_id);
			QDF_ASSERT(0);
			return status;
		}
		status = qdf_mc_timer_start(&ser_timer->timer,
					    cmd->cmd_timeout_duration);
		if (QDF_IS_STATUS_ERROR(status)) {
			serialization_err("Failed to start timer cmdid [%d]",
					cmd->cmd_id);
			wlan_serialization_timer_destroy(ser_timer);
			QDF_ASSERT(0);
			return status;
		}
		serialization_debug("Started timer for cmd: type[%d] id[%d]",
				    cmd->cmd_type, cmd->cmd_id);
	} else {
		serialization_err("Failed to start timer for cmd: type[%d] id[%d]",
				  cmd->cmd_type, cmd->cmd_id);
	}

	return status;
}

/**
 * wlan_serialization_active_scan_cmd_count_handler() - count active scan cmds
 * @psoc: pointer to soc strucutre
 * @obj : pointer to pdev object
 * @arg: pointer to argument
 *
 * This API will be called while iterating each pdev object and it will count
 * number of scan commands present in that pdev object's active queue. count
 * will be updated in *arg
 *
 * Return: none
 */
static void
wlan_serialization_active_scan_cmd_count_handler(struct wlan_objmgr_psoc *psoc,
						 void *obj, void *arg)
{
	struct wlan_objmgr_pdev *pdev = obj;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj;
	uint32_t *count = arg;

	if (!pdev) {
		serialization_err("invalid pdev");
		return;
	}

	ser_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_SERIALIZATION);
	*count += wlan_serialization_list_size(&ser_pdev_obj->active_scan_list,
					       ser_pdev_obj);
}

/**
 * wlan_serialization_is_active_scan_cmd_allowed() - find if scan cmd allowed
 * @pdev: pointer to pdev object
 *
 * This API will be called to find out if active scan cmd is allowed. It has
 * to iterate through all pdev to find out total number of active scan cmds.
 * If total number of active scan cmds reach to allowed threshold then don't
 * allow more scan cmd.
 *
 * Return: true or false
 */
static bool
wlan_serialization_is_active_scan_cmd_allowed(struct wlan_objmgr_pdev *pdev)
{
	uint32_t count = 0;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		serialization_err("invalid pdev");
		return false;
	}

	psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc) {
		serialization_err("invalid psoc");
		return false;
	}

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
			wlan_serialization_active_scan_cmd_count_handler,
			&count, 1, WLAN_SERIALIZATION_ID);
	if (count < ucfg_scan_get_max_active_scans(psoc)) {
		serialization_debug("count is [%d]", count);
		return true;
	}

	return false;
}

/**
 * wlan_serialization_is_active_nonscan_cmd_allowed() - find if cmd allowed
 * @pdev: pointer to pdev object
 *
 * This API will be called to find out if non scan cmd is allowed.
 *
 * Return: true or false
 */
static bool
wlan_serialization_is_active_nonscan_cmd_allowed(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj;

	if (!pdev) {
		serialization_err("invalid pdev");
		return false;
	}

	ser_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_SERIALIZATION);

	if (!ser_pdev_obj) {
		serialization_err("invalid ser_pdev_obj");
		return false;
	}

	if (wlan_serialization_list_empty(&ser_pdev_obj->active_list,
					  ser_pdev_obj))
		return true;

	return false;
}

bool
wlan_serialization_is_active_cmd_allowed(struct wlan_serialization_command *cmd)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_serialization_get_pdev_from_cmd(cmd);
	if (!pdev) {
		serialization_err("NULL pdev");
		return false;
	}

	if (cmd->cmd_type < WLAN_SER_CMD_NONSCAN)
		return wlan_serialization_is_active_scan_cmd_allowed(pdev);
	else
		return wlan_serialization_is_active_nonscan_cmd_allowed(pdev);
}

bool
wlan_serialization_is_scan_cmd_allowed(struct wlan_objmgr_psoc *psoc,
				       struct wlan_serialization_pdev_priv_obj
				       *ser_pdev_obj)
{
	uint32_t active_count, pending_count;
	uint32_t max_scan_commands_allowed;

	if (!psoc || !ser_pdev_obj) {
		serialization_err("invalid psoc or serialization object");
		return false;
	}

	max_scan_commands_allowed = ucfg_scan_get_max_cmd_allowed();

	active_count =
		wlan_serialization_list_size(&ser_pdev_obj->active_scan_list,
					     ser_pdev_obj);

	pending_count =
		wlan_serialization_list_size(&ser_pdev_obj->pending_scan_list,
					     ser_pdev_obj);

	if ((active_count + pending_count) >= max_scan_commands_allowed) {
		serialization_debug("active scan cmds %d, pending scan cmds %d max allowed %d",
				    active_count, pending_count,
				    max_scan_commands_allowed);
		return false;
	}

	return true;
}

QDF_STATUS wlan_serialization_validate_cmdtype(
		 enum wlan_serialization_cmd_type cmd_type)
{
	serialization_debug("validate cmd_type:%d", cmd_type);

	if (cmd_type < 0 || cmd_type >= WLAN_SER_CMD_MAX) {
		serialization_err("Invalid cmd or comp passed");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_serialization_validate_cmd(
		 enum wlan_umac_comp_id comp_id,
		 enum wlan_serialization_cmd_type cmd_type)
{
	serialization_debug("validate cmd_type:%d, comp_id:%d",
			cmd_type, comp_id);
	if (cmd_type < 0 || comp_id < 0 ||
			cmd_type >= WLAN_SER_CMD_MAX ||
			comp_id >= WLAN_UMAC_COMP_ID_MAX) {
		serialization_err("Invalid cmd or comp passed");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

static void wlan_serialization_release_list_cmds(
		struct wlan_serialization_pdev_priv_obj *ser_pdev_obj,
		qdf_list_t *list)
{
	qdf_list_node_t *node = NULL;

	while (!wlan_serialization_list_empty(list, ser_pdev_obj)) {
		wlan_serialization_remove_front(list, &node, ser_pdev_obj);
		wlan_serialization_insert_back(
				&ser_pdev_obj->global_cmd_pool_list,
				node, ser_pdev_obj);
	}

	return;
}

void wlan_serialization_destroy_list(
		struct wlan_serialization_pdev_priv_obj *ser_pdev_obj,
		qdf_list_t *list)
{
	wlan_serialization_release_list_cmds(ser_pdev_obj, list);
	qdf_list_destroy(list);
}

struct wlan_serialization_psoc_priv_obj *wlan_serialization_get_psoc_priv_obj(
		struct wlan_objmgr_psoc *psoc)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	ser_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_SERIALIZATION);

	return ser_soc_obj;
}

struct wlan_serialization_pdev_priv_obj *wlan_serialization_get_pdev_priv_obj(
		struct wlan_objmgr_pdev *pdev)
{
	struct wlan_serialization_pdev_priv_obj *obj;
	obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
					WLAN_UMAC_COMP_SERIALIZATION);

	return obj;
}

struct wlan_serialization_psoc_priv_obj *
wlan_serialization_get_psoc_obj(struct wlan_serialization_command *cmd)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj = NULL;
	struct wlan_objmgr_psoc *psoc;

	if (!cmd->vdev)
		return ser_soc_obj;

	psoc = wlan_vdev_get_psoc(cmd->vdev);
	ser_soc_obj = wlan_serialization_get_psoc_priv_obj(psoc);

	return ser_soc_obj;
}

bool wlan_serialization_is_cmd_in_vdev_list(struct wlan_objmgr_vdev *vdev,
					    qdf_list_t *queue)
{
	uint32_t queuelen;
	qdf_list_node_t *nnode = NULL;
	qdf_list_node_t *pnode;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);
	QDF_STATUS status;
	struct wlan_serialization_command_list *cmd_list = NULL;
	bool match_found = false;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	queuelen = qdf_list_size(queue);
	if (!queuelen) {
		wlan_serialization_release_lock(
					&ser_pdev_obj->pdev_ser_list_lock);
		serialization_debug("queue empty");
		return false;
	}

	status = qdf_list_peek_front(queue, &nnode);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_serialization_release_lock(
					&ser_pdev_obj->pdev_ser_list_lock);
		serialization_debug("failed to get first node");
		return false;
	}
	while (nnode) {

		cmd_list = qdf_container_of(nnode,
				    struct wlan_serialization_command_list,
				    node);
		if (cmd_list->cmd.vdev == vdev) {
			match_found = true;
			break;
		}

		pnode = nnode;
		status = qdf_list_peek_next(queue, pnode, &nnode);
		if (QDF_IS_STATUS_ERROR(status))
			break;
	}
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return match_found;
}

bool wlan_serialization_is_cmd_in_pdev_list(
			struct wlan_objmgr_pdev *pdev,
			qdf_list_t *queue)
{
	uint32_t queuelen;
	qdf_list_node_t *nnode = NULL;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);
	QDF_STATUS status;

	queuelen = wlan_serialization_list_size(queue, ser_pdev_obj);
	if (!queuelen) {
		serialization_debug("queue empty");
		return false;
	}

	while (queuelen--) {
		status = wlan_serialization_get_cmd_from_queue(queue, &nnode,
							       ser_pdev_obj);
		if (status != QDF_STATUS_SUCCESS)
			break;
		if (wlan_serialization_match_cmd_pdev(nnode, pdev))
			return true;
	}

	return false;
}

enum wlan_serialization_cmd_status
wlan_serialization_is_cmd_in_active_pending(bool cmd_in_active,
					    bool cmd_in_pending)
{
	if (cmd_in_active && cmd_in_pending)
		return WLAN_SER_CMDS_IN_ALL_LISTS;
	else if (cmd_in_active)
		return WLAN_SER_CMD_IN_ACTIVE_LIST;
	else if (cmd_in_pending)
		return WLAN_SER_CMD_IN_PENDING_LIST;
	else
		return WLAN_SER_CMD_NOT_FOUND;
}

static bool wlan_serialization_is_cmd_present_in_given_queue(qdf_list_t *queue,
			struct wlan_serialization_command *cmd,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	uint32_t qsize;
	QDF_STATUS status;
	struct wlan_serialization_command_list *cmd_list = NULL;
	qdf_list_node_t *nnode = NULL, *pnode = NULL;
	bool found = false;

	qsize = wlan_serialization_list_size(queue, ser_pdev_obj);
	while (qsize--) {
		if (!cmd_list)
			status = wlan_serialization_peek_front(queue, &nnode,
							       ser_pdev_obj);
		else
			status = wlan_serialization_peek_next(queue, pnode,
							      &nnode,
							      ser_pdev_obj);

		if (status != QDF_STATUS_SUCCESS)
			break;

		pnode = nnode;
		cmd_list = qdf_container_of(nnode,
				struct wlan_serialization_command_list, node);
		if (wlan_serialization_match_cmd_id_type(nnode, cmd,
							 ser_pdev_obj) &&
			wlan_serialization_match_cmd_vdev(nnode, cmd->vdev)) {
			found = true;
			break;
		}
		nnode = NULL;
	}

	return found;
}

bool wlan_serialization_is_cmd_present_queue(
			struct wlan_serialization_command *cmd,
			uint8_t is_active_queue)
{
	qdf_list_t *queue;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj;

	if (!cmd) {
		serialization_err("invalid params");
		return false;
	}
	pdev = wlan_serialization_get_pdev_from_cmd(cmd);
	if (!pdev) {
		serialization_err("invalid pdev");
		return false;
	}
	ser_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
					WLAN_UMAC_COMP_SERIALIZATION);
	if (!ser_pdev_obj) {
		serialization_err("invalid ser_pdev_obj");
		return false;
	}
	if (!is_active_queue) {
		if (cmd->cmd_type < WLAN_SER_CMD_NONSCAN)
			queue = &ser_pdev_obj->pending_scan_list;
		else
			queue = &ser_pdev_obj->pending_list;
	} else {
		if (cmd->cmd_type < WLAN_SER_CMD_NONSCAN)
			queue = &ser_pdev_obj->active_scan_list;
		else
			queue = &ser_pdev_obj->active_list;
	}

	return wlan_serialization_is_cmd_present_in_given_queue(queue, cmd,
								ser_pdev_obj);
}

bool wlan_serialization_list_empty(
			qdf_list_t *queue,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	bool is_empty;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	if (qdf_list_empty(queue))
		is_empty = true;
	else
		is_empty = false;
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return is_empty;
}

uint32_t wlan_serialization_list_size(
			qdf_list_t *queue,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	uint32_t size;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	size = qdf_list_size(queue);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return size;
}

QDF_STATUS wlan_serialization_remove_front(
			qdf_list_t *list,
			qdf_list_node_t **node,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_remove_front(list, node);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

QDF_STATUS wlan_serialization_remove_node(
			qdf_list_t *list,
			qdf_list_node_t *node,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_remove_node(list, node);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

QDF_STATUS wlan_serialization_insert_front(
			qdf_list_t *list,
			qdf_list_node_t *node,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_insert_front(list, node);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

QDF_STATUS wlan_serialization_insert_back(
			qdf_list_t *list,
			qdf_list_node_t *node,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_insert_back(list, node);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

QDF_STATUS wlan_serialization_peek_front(
			qdf_list_t *list,
			qdf_list_node_t **node,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_peek_front(list, node);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

QDF_STATUS wlan_serialization_peek_next(
			qdf_list_t *list,
			qdf_list_node_t *node1, qdf_list_node_t **node2,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	QDF_STATUS status;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	status = qdf_list_peek_next(list, node1, node2);
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return status;
}

bool wlan_serialization_match_cmd_scan_id(
			qdf_list_node_t *nnode,
			struct wlan_serialization_command **cmd,
			uint16_t scan_id, struct wlan_objmgr_vdev *vdev,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	struct wlan_serialization_command_list *cmd_list = NULL;
	bool match_found = false;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	cmd_list = qdf_container_of(nnode,
				    struct wlan_serialization_command_list,
				    node);
	if ((cmd_list->cmd.cmd_id == scan_id) &&
	    (cmd_list->cmd.vdev == vdev)) {
		*cmd = &cmd_list->cmd;
		match_found = true;
	};
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return match_found;
}

bool wlan_serialization_match_cmd_id_type(
			qdf_list_node_t *nnode,
			struct wlan_serialization_command *cmd,
			struct wlan_serialization_pdev_priv_obj *ser_pdev_obj)
{
	struct wlan_serialization_command_list *cmd_list = NULL;
	bool match_found = true;

	if (!cmd)
		return false;
	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	cmd_list = qdf_container_of(nnode,
				    struct wlan_serialization_command_list,
				    node);
	if ((cmd_list->cmd.cmd_id != cmd->cmd_id) ||
	    (cmd_list->cmd.cmd_type != cmd->cmd_type)) {
		match_found = false;
	};
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return match_found;
}

bool wlan_serialization_match_cmd_vdev(qdf_list_node_t *nnode,
				       struct wlan_objmgr_vdev *vdev)
{
	struct wlan_serialization_command_list *cmd_list = NULL;
	bool match_found = false;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	cmd_list = qdf_container_of(nnode,
				    struct wlan_serialization_command_list,
				    node);
	if (cmd_list->cmd.vdev == vdev)
		match_found = true;
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return match_found;
}

bool wlan_serialization_match_cmd_pdev(qdf_list_node_t *nnode,
				       struct wlan_objmgr_pdev *pdev)
{
	struct wlan_serialization_command_list *cmd_list = NULL;
	bool match_found = false;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);
	struct wlan_objmgr_pdev *node_pdev = NULL;

	wlan_serialization_acquire_lock(&ser_pdev_obj->pdev_ser_list_lock);
	cmd_list = qdf_container_of(nnode,
				    struct wlan_serialization_command_list,
				    node);
	node_pdev = wlan_vdev_get_pdev(cmd_list->cmd.vdev);
	if (node_pdev == pdev)
		match_found = true;
	wlan_serialization_release_lock(&ser_pdev_obj->pdev_ser_list_lock);

	return match_found;
}

#ifdef WLAN_CMD_SERIALIZATION_LOCKING
QDF_STATUS
wlan_serialization_acquire_lock(qdf_spinlock_t *lock)
{
	qdf_spin_lock_bh(lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_release_lock(qdf_spinlock_t *lock)
{
	qdf_spin_unlock_bh(lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_create_lock(qdf_spinlock_t *lock)
{
	qdf_spinlock_create(lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_destroy_lock(qdf_spinlock_t *lock)
{
	qdf_spinlock_destroy(lock);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS
wlan_serialization_acquire_lock(qdf_spinlock_t *lock)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_release_lock(qdf_spinlock_t *lock)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_create_lock(qdf_spinlock_t *lock)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_destroy_lock(qdf_spinlock_t *lock)
{
	return QDF_STATUS_SUCCESS;
}
#endif
