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

/*
 * DOC: contains scan manager functionality
 */

#include <wlan_serialization_api.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_tgt_api.h>
#include "wlan_scan_main.h"
#include "wlan_scan_manager.h"
#include "wlan_utility.h"
#ifdef FEATURE_WLAN_SCAN_PNO
#include <host_diag_core_event.h>
#endif

QDF_STATUS
scm_scan_free_scan_request_mem(struct scan_start_request *req)
{
	void *ie;

	if (!req) {
		scm_err("null request");
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	/* Free vendor(extra) ie */
	ie = req->scan_req.extraie.ptr;
	if (ie) {
		req->scan_req.extraie.ptr = NULL;
		req->scan_req.extraie.len = 0;
		qdf_mem_free(ie);
	}

	/* Free htcap ie */
	ie = req->scan_req.htcap.ptr;
	if (ie) {
		req->scan_req.htcap.len = 0;
		req->scan_req.htcap.ptr = NULL;
		qdf_mem_free(ie);
	}

	/* Free vhtcap ie */
	ie = req->scan_req.vhtcap.ptr;
	if (ie) {
		req->scan_req.vhtcap.len = 0;
		req->scan_req.vhtcap.ptr = NULL;
		qdf_mem_free(ie);
	}
	/* free scan_start_request memory */
	qdf_mem_free(req);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
scm_scan_get_pdev_global_event_handlers(struct scan_event_listeners *listeners,
		struct pdev_scan_ev_handler *pdev_ev_handler)
{
	uint32_t i;
	struct cb_handler *cb_handlers  = &(pdev_ev_handler->cb_handlers[0]);

	for (i = 0; i < MAX_SCAN_EVENT_HANDLERS_PER_PDEV; i++, cb_handlers++) {
		if ((cb_handlers->func) &&
		    (listeners->count < MAX_SCAN_EVENT_LISTENERS)) {
			listeners->cb[listeners->count].func =
				cb_handlers->func;
			listeners->cb[listeners->count].arg =
				cb_handlers->arg;
			listeners->count++;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
scm_scan_get_requester_event_handler(struct scan_event_listeners *listeners,
		struct scan_requester_info *requesters,
		wlan_scan_requester requester_id)
{
	uint32_t idx;
	struct cb_handler *ev_handler;

	idx = requester_id & WLAN_SCAN_REQUESTER_ID_PREFIX;
	if (idx != WLAN_SCAN_REQUESTER_ID_PREFIX)
		return QDF_STATUS_SUCCESS;

	idx = requester_id & WLAN_SCAN_REQUESTER_ID_MASK;
	if (idx < WLAN_MAX_REQUESTORS) {
		ev_handler = &(requesters[idx].ev_handler);
		if (ev_handler->func) {
			if (listeners->count < MAX_SCAN_EVENT_LISTENERS) {
				listeners->cb[listeners->count].func =
							     ev_handler->func;
				listeners->cb[listeners->count].arg =
							     ev_handler->arg;
				listeners->count++;
			}
		}
		return QDF_STATUS_SUCCESS;
	} else {
		scm_err("invalid requester id");
		return QDF_STATUS_E_INVAL;
	}

}

static void scm_scan_post_event(struct wlan_objmgr_vdev *vdev,
		struct scan_event *event)
{
	uint32_t i = 0;
	struct wlan_scan_obj *scan;
	struct pdev_scan_ev_handler *pdev_ev_handler;
	struct cb_handler *cb_handlers;
	struct scan_requester_info *requesters;
	struct scan_event_listeners *listeners;

	if (!vdev || !event) {
		scm_err("vdev: 0x%pK, event: 0x%pK", vdev, event);
		return;
	}
	if (!event->requester) {
		scm_err("invalid requester id");
		QDF_ASSERT(0);
	}
	scan = wlan_vdev_get_scan_obj(vdev);
	pdev_ev_handler = wlan_vdev_get_pdev_scan_ev_handlers(vdev);
	if (!pdev_ev_handler)
		return;
	cb_handlers = &(pdev_ev_handler->cb_handlers[0]);
	requesters = scan->requesters;

	listeners = qdf_mem_malloc_atomic(sizeof(*listeners));
	if (!listeners) {
		scm_warn("couldn't allocate listeners list");
		return;
	}

	/* initialize number of listeners */
	listeners->count = 0;

	/*
	 * Initiator of scan request decides which all scan events
	 * he is interested in and FW will send only those scan events
	 * to host driver.
	 * All the events received by scan module will be notified
	 * to all registered handlers.
	 */

	qdf_spin_lock_bh(&scan->lock);
	/* find all global scan event handlers on this pdev */
	scm_scan_get_pdev_global_event_handlers(listeners, pdev_ev_handler);
	/* find owner who triggered this scan request */
	scm_scan_get_requester_event_handler(listeners, requesters,
			event->requester);
	qdf_spin_unlock_bh(&scan->lock);

	/* notify all interested handlers */
	for (i = 0; i < listeners->count; i++)
		listeners->cb[i].func(vdev, event, listeners->cb[i].arg);
	qdf_mem_free(listeners);
}

static QDF_STATUS
scm_release_serialization_command(struct wlan_objmgr_vdev *vdev,
		uint32_t scan_id)
{
	struct wlan_serialization_queued_cmd_info cmd = {0};

	cmd.requestor = WLAN_UMAC_COMP_SCAN;
	cmd.cmd_type = WLAN_SER_CMD_SCAN;
	cmd.cmd_id = scan_id;
	cmd.req_type = WLAN_SER_CANCEL_SINGLE_SCAN;
	cmd.vdev = vdev;
	cmd.queue_type = WLAN_SERIALIZATION_ACTIVE_QUEUE;

	/* Inform serialization for command completion */
	wlan_serialization_remove_cmd(&cmd);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
scm_post_internal_scan_complete_event(struct scan_start_request *req,
		enum scan_completion_reason reason)
{
	struct scan_event event = {0, };

	/* prepare internal scan complete event */
	event.type = SCAN_EVENT_TYPE_COMPLETED;
	event.reason = reason;
	event.chan_freq = 0; /* Invalid frequency */
	event.vdev_id =  req->scan_req.vdev_id;
	event.requester = req->scan_req.scan_req_id;
	event.scan_id = req->scan_req.scan_id;
	/* Fill scan_start_request used to trigger this scan */
	event.scan_start_req = req;
	/* post scan event to registered handlers */
	scm_scan_post_event(req->vdev, &event);

	return QDF_STATUS_SUCCESS;
}

static inline struct pdev_scan_info *
scm_scan_get_pdev_priv_info(uint8_t pdev_id, struct wlan_scan_obj *scan_obj)
{
	return &scan_obj->pdev_info[pdev_id];
}

static QDF_STATUS
scm_update_last_scan_time(struct scan_start_request *req)
{
	uint8_t pdev_id;
	struct wlan_scan_obj *scan_obj;
	struct pdev_scan_info *pdev_scan_info;

	scan_obj = wlan_vdev_get_scan_obj(req->vdev);
	pdev_id = wlan_scan_vdev_get_pdev_id(req->vdev);
	pdev_scan_info = scm_scan_get_pdev_priv_info(pdev_id, scan_obj);
	/* update last scan start time */
	pdev_scan_info->last_scan_time = qdf_system_ticks();

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
scm_activate_scan_request(struct scan_start_request *req)
{
	QDF_STATUS status;

	status = tgt_scan_start(req);
	if (status != QDF_STATUS_SUCCESS) {
		scm_err("tgt_scan_start failed, status: %d", status);
		/* scan could not be started and hence
		 * we will not receive any completions.
		 * post scan cancelled
		 */
		scm_post_internal_scan_complete_event(req,
				SCAN_REASON_CANCELLED);
		return status;
	}
	/* save last scan start time */
	status = scm_update_last_scan_time(req);

	return status;
}

static QDF_STATUS
scm_cancel_scan_request(struct scan_start_request *req)
{
	struct scan_cancel_request cancel_req = {0, };
	QDF_STATUS status;

	cancel_req.vdev = req->vdev;
	cancel_req.cancel_req.scan_id = req->scan_req.scan_id;
	cancel_req.cancel_req.requester = req->scan_req.scan_req_id;
	cancel_req.cancel_req.req_type = WLAN_SCAN_CANCEL_SINGLE;
	cancel_req.cancel_req.vdev_id = req->scan_req.vdev_id;
	/* send scan cancel to fw */
	status = tgt_scan_cancel(&cancel_req);
	if (status != QDF_STATUS_SUCCESS)
		scm_err("tgt_scan_cancel failed: status: %d, scanid: %d",
			status, req->scan_req.scan_id);
	/* notify event handler about scan cancellation */
	scm_post_internal_scan_complete_event(req, SCAN_REASON_CANCELLED);

	return status;
}

static QDF_STATUS
scm_scan_serialize_callback(struct wlan_serialization_command *cmd,
	enum wlan_serialization_cb_reason reason)
{
	struct scan_start_request *req;
	QDF_STATUS status;

	if (!cmd) {
		scm_err("cmd is NULL, reason: %d", reason);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!cmd->umac_cmd) {
		scm_err("cmd->umac_cmd is NULL , reason: %d", reason);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = cmd->umac_cmd;
	if (!req->vdev) {
		scm_err("NULL vdev. req:0x%pK, reason:%d\n", req, reason);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	qdf_mtrace(QDF_MODULE_ID_SERIALIZATION, QDF_MODULE_ID_SCAN, reason,
		   req->scan_req.vdev_id, req->scan_req.scan_id);

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list
		 * modify the params if required for concurency case.
		 */
		status = scm_activate_scan_request(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list.
		 * notify registered scan event handlers with
		 * status completed and reason cancelled.
		 */
		status = scm_post_internal_scan_complete_event(req,
				SCAN_REASON_CANCELLED);
		break;

	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* active command timed out.
		 * prepare internal scan cancel request
		 */
		status = scm_cancel_scan_request(req);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * Release vdev reference and free scan_start_request memory
		 */
		cmd->umac_cmd = NULL;
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
		status = scm_scan_free_scan_request_mem(req);
		break;

	default:
		/* Do nothing but logging */
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

static inline void scm_print_scan_req_info(struct scan_req_params *req)
{
	uint32_t buff_len;
	char *chan_buff;
	uint32_t len = 0;
	uint8_t idx, count = 0;
	struct chan_list *chan_lst;
#define MAX_SCAN_FREQ_TO_PRINT 60

	scm_nofl_debug("Scan start: scan id %d vdev %d Dwell time: act %d pass %d act_2G %d, probe time %d n_probes %d flags %x ext_flag %x events %x policy %d wide_bw %d pri %d",
		       req->scan_id, req->vdev_id, req->dwell_time_active,
		       req->dwell_time_passive, req->dwell_time_active_2g,
		       req->repeat_probe_time, req->n_probes, req->scan_flags,
		       req->scan_ctrl_flags_ext, req->scan_events,
		       req->scan_policy_type, req->scan_f_wide_band,
		       req->scan_priority);

	for (idx = 0; idx < req->num_ssids; idx++)
		scm_nofl_debug("SSID[%d]: %.*s", idx, req->ssid[idx].length,
			       req->ssid[idx].ssid);

	chan_lst  = &req->chan_list;

	if (!chan_lst->num_chan)
		return;
	/*
	 * Buffer of (num channl * 5) + 1  to consider the 4 char freq and
	 * 1 space after it for each channel and 1 to end the string with NULL.
	 */
	buff_len =
		(QDF_MIN(MAX_SCAN_FREQ_TO_PRINT, chan_lst->num_chan) * 5) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return;
	scm_nofl_debug("Total freq %d", chan_lst->num_chan);
	for (idx = 0; idx < chan_lst->num_chan; idx++) {
		len += qdf_scnprintf(chan_buff + len, buff_len - len, "%d ",
				     chan_lst->chan[idx].freq);
		count++;
		if (count >= MAX_SCAN_FREQ_TO_PRINT) {
			/* Print the MAX_SCAN_FREQ_TO_PRINT channels */
			scm_nofl_debug("Freq list: %s", chan_buff);
			len = 0;
			count = 0;
		}
	}
	if (len)
		scm_nofl_debug("Freq list: %s", chan_buff);

	qdf_mem_free(chan_buff);
}

QDF_STATUS
scm_scan_start_req(struct scheduler_msg *msg)
{
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;
	struct scan_start_request *req = NULL;
	struct wlan_scan_obj *scan_obj;
	struct scan_vdev_obj *scan_vdev_priv_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!msg) {
		scm_err("msg received is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (!msg->bodyptr) {
		scm_err("bodyptr is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = msg->bodyptr;
	scan_obj = wlan_vdev_get_scan_obj(req->vdev);
	if (!scan_obj) {
		scm_debug("Couldn't find scan object");
		status = QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	if (!scan_obj->enable_scan) {
		scm_err("scan disabled, rejecting the scan req");
		status = QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	scan_vdev_priv_obj = wlan_get_vdev_scan_obj(req->vdev);
	if (!scan_vdev_priv_obj) {
		scm_debug("Couldn't find scan priv object");
		status = QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	if (scan_vdev_priv_obj->is_vdev_delete_in_progress) {
		scm_err("Can't allow scan on vdev_id:%d",
			wlan_vdev_get_id(req->vdev));
		status = QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	scm_print_scan_req_info(&req->scan_req);

	cmd.cmd_type = WLAN_SER_CMD_SCAN;
	cmd.cmd_id = req->scan_req.scan_id;
	cmd.cmd_cb = scm_scan_serialize_callback;
	cmd.umac_cmd = req;
	cmd.source = WLAN_UMAC_COMP_SCAN;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = req->scan_req.max_scan_time +
		SCAN_TIMEOUT_GRACE_PERIOD;
	cmd.vdev = req->vdev;

	if (scan_obj->disable_timeout)
		cmd.cmd_timeout_duration = 0;

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_SERIALIZATION,
		   WLAN_SER_CMD_SCAN, req->vdev->vdev_objmgr.vdev_id,
		   req->scan_req.scan_id);

	ser_cmd_status = wlan_serialization_request(&cmd);
	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list.Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		scm_debug("ser cmd status %d", ser_cmd_status);
		goto err;
	}

	return status;
err:
	/*
	 * notify registered scan event handlers
	 * about internal error
	 */
	scm_post_internal_scan_complete_event(req,
					      SCAN_REASON_INTERNAL_FAILURE);
	/*
	 * cmd can't be serviced.
	 * release vdev reference and free scan_start_request memory
	 */
	if (req) {
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
		scm_scan_free_scan_request_mem(req);
	}

	return status;
}

static inline enum wlan_serialization_cancel_type
get_serialization_cancel_type(enum scan_cancel_req_type type)
{
	enum wlan_serialization_cancel_type serialization_type;

	switch (type) {
	case WLAN_SCAN_CANCEL_SINGLE:
		serialization_type = WLAN_SER_CANCEL_SINGLE_SCAN;
		break;
	case WLAN_SCAN_CANCEL_VDEV_ALL:
		serialization_type = WLAN_SER_CANCEL_VDEV_SCANS;
		break;
	case WLAN_SCAN_CANCEL_PDEV_ALL:
		serialization_type = WLAN_SER_CANCEL_PDEV_SCANS;
		break;
	case WLAN_SCAN_CANCEL_HOST_VDEV_ALL:
		serialization_type = WLAN_SER_CANCEL_VDEV_HOST_SCANS;
		break;
	default:
		QDF_ASSERT(0);
		scm_warn("invalid scan_cancel_req_type: %d", type);
		serialization_type = WLAN_SER_CANCEL_PDEV_SCANS;
		break;
	}

	return serialization_type;
}

QDF_STATUS
scm_scan_cancel_req(struct scheduler_msg *msg)
{
	struct wlan_serialization_queued_cmd_info cmd = {0,};
	struct wlan_serialization_command ser_cmd = {0,};
	enum wlan_serialization_cmd_status ser_cmd_status;
	struct scan_cancel_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!msg) {
		scm_err("msg received is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (!msg->bodyptr) {
		scm_err("Bodyptr is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = msg->bodyptr;
	/*
	 * If requester wants to wait for target scan cancel event
	 * instead of internally generated cancel event, just check
	 * which queue this scan request belongs to and send scan
	 * cancel request to FW accordingly.
	 * Else generate internal scan cancel event and notify
	 * handlers and free scan request resources.
	 */
	if (req->wait_tgt_cancel &&
			(req->cancel_req.req_type == WLAN_SCAN_CANCEL_SINGLE)) {
		ser_cmd.cmd_type = WLAN_SER_CMD_SCAN;
		ser_cmd.cmd_id = req->cancel_req.scan_id;
		ser_cmd.cmd_cb = NULL;
		ser_cmd.umac_cmd = NULL;
		ser_cmd.source = WLAN_UMAC_COMP_SCAN;
		ser_cmd.is_high_priority = false;
		ser_cmd.vdev = req->vdev;
		if (wlan_serialization_is_cmd_present_in_active_queue(NULL, &ser_cmd))
			ser_cmd_status = WLAN_SER_CMD_IN_ACTIVE_LIST;
		else if (wlan_serialization_is_cmd_present_in_pending_queue(NULL, &ser_cmd))
			ser_cmd_status = WLAN_SER_CMD_IN_PENDING_LIST;
		else
			ser_cmd_status = WLAN_SER_CMD_NOT_FOUND;
	} else {
		cmd.requestor = 0;
		cmd.cmd_type = WLAN_SER_CMD_SCAN;
		cmd.cmd_id = req->cancel_req.scan_id;
		cmd.vdev = req->vdev;
		cmd.queue_type = WLAN_SERIALIZATION_ACTIVE_QUEUE |
			WLAN_SERIALIZATION_PENDING_QUEUE;
		cmd.req_type = get_serialization_cancel_type(req->cancel_req.req_type);

		ser_cmd_status = wlan_serialization_cancel_request(&cmd);
	}

	scm_debug("status: %d, reqid: %d, scanid: %d, vdevid: %d, type: %d",
		ser_cmd_status, req->cancel_req.requester,
		req->cancel_req.scan_id, req->cancel_req.vdev_id,
		req->cancel_req.req_type);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_IN_PENDING_LIST:
		/* do nothing */
		break;
	case WLAN_SER_CMD_IN_ACTIVE_LIST:
	case WLAN_SER_CMDS_IN_ALL_LISTS:
		/* send wmi scan cancel to fw */
		status = tgt_scan_cancel(req);
		break;
	case WLAN_SER_CMD_NOT_FOUND:
		/* do nothing */
		break;
	default:
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	/* Release vdev reference and scan cancel request
	 * processing is complete
	 */
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
	/* Free cancel request memory */
	qdf_mem_free(req);

	return status;
}

#ifdef FEATURE_WLAN_SCAN_PNO
static QDF_STATUS
scm_pno_event_handler(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event)
{
	struct scan_vdev_obj *scan_vdev_obj;
	struct wlan_scan_obj *scan_psoc_obj;
	scan_event_handler pno_cb;
	void *cb_arg;

	scan_vdev_obj = wlan_get_vdev_scan_obj(vdev);
	scan_psoc_obj = wlan_vdev_get_scan_obj(vdev);
	if (!scan_vdev_obj || !scan_psoc_obj) {
		scm_err("null scan_vdev_obj %pK scan_obj %pK",
			scan_vdev_obj, scan_psoc_obj);
		return QDF_STATUS_E_INVAL;
	}

	switch (event->type) {
	case SCAN_EVENT_TYPE_NLO_COMPLETE:
		if (!scan_vdev_obj->pno_match_evt_received)
			return QDF_STATUS_SUCCESS;
		qdf_wake_lock_release(&scan_psoc_obj->pno_cfg.pno_wake_lock,
			WIFI_POWER_EVENT_WAKELOCK_PNO);
		qdf_wake_lock_timeout_acquire(
			&scan_psoc_obj->pno_cfg.pno_wake_lock,
			SCAN_PNO_SCAN_COMPLETE_WAKE_LOCK_TIMEOUT);
		scan_vdev_obj->pno_match_evt_received = false;
		break;
	case SCAN_EVENT_TYPE_NLO_MATCH:
		scan_vdev_obj->pno_match_evt_received = true;
		qdf_wake_lock_timeout_acquire(
			&scan_psoc_obj->pno_cfg.pno_wake_lock,
			SCAN_PNO_MATCH_WAKE_LOCK_TIMEOUT);
		return QDF_STATUS_SUCCESS;
	default:
		return QDF_STATUS_E_INVAL;
	}
	qdf_spin_lock_bh(&scan_psoc_obj->lock);
	pno_cb = scan_psoc_obj->pno_cfg.pno_cb.func;
	cb_arg = scan_psoc_obj->pno_cfg.pno_cb.arg;
	qdf_spin_unlock_bh(&scan_psoc_obj->lock);

	if (pno_cb)
		pno_cb(vdev, event, cb_arg);

	return QDF_STATUS_SUCCESS;
}
#else

static QDF_STATUS
scm_pno_event_handler(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * scm_scan_update_scan_event() - update scan event
 * @scan: scan object
 * @event: scan event
 * @scan_start_req: scan_start_req used for triggering scan
 *
 * update scan params in scan event
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
scm_scan_update_scan_event(struct wlan_scan_obj *scan,
		struct scan_event *event,
		struct scan_start_request *scan_start_req)
{
	if (!event)
		return QDF_STATUS_E_NULL_VALUE;

	if (!scan || !scan_start_req) {
		event->scan_start_req = NULL;
		return QDF_STATUS_E_NULL_VALUE;
	}
	/* copy scan start request to pass back buffer */
	qdf_mem_copy(&scan->scan_start_request_buff, scan_start_req,
			sizeof(struct scan_start_request));
	/* reset all pointers */
	scan->scan_start_request_buff.scan_req.extraie.ptr = NULL;
	scan->scan_start_request_buff.scan_req.extraie.len = 0;
	scan->scan_start_request_buff.scan_req.htcap.ptr = NULL;
	scan->scan_start_request_buff.scan_req.htcap.len = 0;
	scan->scan_start_request_buff.scan_req.vhtcap.ptr = NULL;
	scan->scan_start_request_buff.scan_req.vhtcap.len = 0;

	event->scan_start_req = &scan->scan_start_request_buff;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
scm_scan_event_handler(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev;
	struct scan_event *event;
	struct scan_event_info *event_info;
	struct wlan_serialization_command cmd = {0,};
	struct wlan_serialization_command *queued_cmd;
	struct scan_start_request *scan_start_req;
	struct wlan_scan_obj *scan;

	if (!msg) {
		scm_err("NULL msg received ");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (!msg->bodyptr) {
		scm_err("NULL scan event received");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	event_info = msg->bodyptr;
	vdev = event_info->vdev;
	event = &(event_info->event);

	scm_debug("vdevid:%d, type:%d, reason:%d, freq:%d, reqstr:%d, scanid:%d",
		  event->vdev_id, event->type, event->reason, event->chan_freq,
		  event->requester, event->scan_id);
	/*
	 * NLO requests are never queued, so post NLO events
	 * without checking for their presence in active queue.
	 */
	switch (event->type) {
	case SCAN_EVENT_TYPE_NLO_COMPLETE:
	case SCAN_EVENT_TYPE_NLO_MATCH:
		scm_pno_event_handler(vdev, event);
		goto exit;
	default:
		break;
	}

	cmd.cmd_type = WLAN_SER_CMD_SCAN;
	cmd.cmd_id = event->scan_id;
	cmd.cmd_cb = NULL;
	cmd.umac_cmd = NULL;
	cmd.source = WLAN_UMAC_COMP_SCAN;
	cmd.is_high_priority = false;
	cmd.vdev = vdev;
	if (!wlan_serialization_is_cmd_present_in_active_queue(NULL, &cmd)) {
		/*
		 * We received scan event for an already completed/cancelled
		 * scan request. Drop this event.
		 */
		scm_debug("Received scan event while request not in active queue");
		goto exit;
	}

	/* Fill scan_start_request used to trigger this scan */
	queued_cmd = wlan_serialization_get_scan_cmd_using_scan_id(
			wlan_vdev_get_psoc(vdev), wlan_vdev_get_id(vdev),
			event->scan_id, true);

	if (!queued_cmd) {
		scm_err("NULL queued_cmd");
		goto exit;
	}
	if (!queued_cmd->umac_cmd) {
		scm_err("NULL umac_cmd");
		goto exit;
	}
	scan_start_req = queued_cmd->umac_cmd;

	if (scan_start_req->scan_req.scan_req_id != event->requester) {
		scm_err("req ID mismatch, scan_req_id:%d, event_req_id:%d",
			scan_start_req->scan_req.scan_req_id, event->requester);
		goto exit;
	}

	scan = wlan_vdev_get_scan_obj(vdev);
	if (scan)
		scm_scan_update_scan_event(scan, event, scan_start_req);

	switch (event->type) {
	case SCAN_EVENT_TYPE_COMPLETED:
		if (event->reason == SCAN_REASON_COMPLETED)
			scm_11d_decide_country_code(vdev);
		/* fall through to release the command */
	case SCAN_EVENT_TYPE_START_FAILED:
	case SCAN_EVENT_TYPE_DEQUEUED:
		scm_release_serialization_command(vdev, event->scan_id);
		break;
	default:
		break;
	}

	/* Notify all interested parties */
	scm_scan_post_event(vdev, event);

exit:
	/* free event info memory */
	qdf_mem_free(event_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SCAN_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS scm_scan_event_flush_callback(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev;
	struct scan_event_info *event_info;
	struct scan_event *event;

	if (!msg || !msg->bodyptr) {
		scm_err("msg or msg->bodyptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	event_info = msg->bodyptr;
	vdev = event_info->vdev;
	event = &event_info->event;

	scm_debug("Flush scan event vdev %d type %d reason %d freq: %d req %d scanid %d",
		  event->vdev_id, event->type, event->reason, event->chan_freq,
		  event->requester, event->scan_id);

	/* free event info memory */
	qdf_mem_free(event_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SCAN_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS scm_bcn_probe_flush_callback(struct scheduler_msg *msg)
{
	struct scan_bcn_probe_event *bcn;

	bcn = msg->bodyptr;

	if (!bcn) {
		scm_err("bcn is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (bcn->psoc)
		wlan_objmgr_psoc_release_ref(bcn->psoc, WLAN_SCAN_ID);
	if (bcn->rx_data)
		qdf_mem_free(bcn->rx_data);
	if (bcn->buf)
		qdf_nbuf_free(bcn->buf);
	qdf_mem_free(bcn);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS scm_scan_start_flush_callback(struct scheduler_msg *msg)
{
	struct scan_start_request *req;

	if (!msg || !msg->bodyptr) {
		scm_err("msg or msg->bodyptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = msg->bodyptr;
	scm_post_internal_scan_complete_event(req, SCAN_REASON_CANCELLED);
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
	scm_scan_free_scan_request_mem(req);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS scm_scan_cancel_flush_callback(struct scheduler_msg *msg)
{
	struct scan_cancel_request *req;

	if (!msg || !msg->bodyptr) {
		scm_err("msg or msg->bodyptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = msg->bodyptr;
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
	/* Free cancel request memory */
	qdf_mem_free(req);

	return QDF_STATUS_SUCCESS;
}
