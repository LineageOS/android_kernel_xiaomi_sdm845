/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)��All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_detail_threshold.h
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: Set Detail Threshold for all IC
*
************************************************************************/

#ifndef _DETAIL_THRESHOLD_H
#define _DETAIL_THRESHOLD_H

#define TX_NUM_MAX          60
#define RX_NUM_MAX          60
#define NUM_MAX         (TX_NUM_MAX)*(RX_NUM_MAX)
#define MAX_PATH            256
#define BUFFER_LENGTH       512
#define MAX_TEST_ITEM       20
#define MAX_GRAPH_ITEM       20
#define MAX_CHANNEL_NUM 144

#define FORCETOUCH_ROW  1

struct detailthreshold_incell {
	int *invalid_node;
	int *rawdata_test_min;
	int *rawdata_test_max;
	int *rawdata_test_b_frame_min;
	int *rawdata_test_b_frame_max;
	int *cb_test_min;
	int *cb_test_max;
};
struct detailthreshold_mcap {
	unsigned char (*invalid_node)[RX_NUM_MAX];
	unsigned char (*invalid_node_sc)[RX_NUM_MAX];
	int (*rawdata_test_min)[RX_NUM_MAX];
	int (*rawdata_test_max)[RX_NUM_MAX];
	int (*rawdata_test_low_min)[RX_NUM_MAX];
	int (*rawdata_test_low_max)[RX_NUM_MAX];
	int (*rawdata_test_high_min)[RX_NUM_MAX];
	int (*rawdata_test_high_max)[RX_NUM_MAX];
	int (*rx_linearity_test_max)[RX_NUM_MAX];
	int (*tx_linearity_test_max)[RX_NUM_MAX];
	int (*panel_differ_test_max)[RX_NUM_MAX];
	int (*panel_differ_test_min)[RX_NUM_MAX];
	int (*scap_rawdata_on_max)[RX_NUM_MAX];
	int (*scap_rawdata_on_min)[RX_NUM_MAX];
	int (*scap_rawdata_off_max)[RX_NUM_MAX];
	int (*scap_rawdata_off_min)[RX_NUM_MAX];
	short (*scap_cb_test_on_max)[RX_NUM_MAX];
	short (*scap_cb_test_on_min)[RX_NUM_MAX];
	short (*scap_cb_test_off_max)[RX_NUM_MAX];
	short (*scap_cb_test_off_min)[RX_NUM_MAX];
	int (*noise_test_coefficient)[RX_NUM_MAX];

	int ForceTouch_SCapRawDataTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapRawDataTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_ON_Min[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Max[FORCETOUCH_ROW][RX_NUM_MAX];
	int ForceTouch_SCapCbTest_OFF_Min[FORCETOUCH_ROW][RX_NUM_MAX];
};

struct detailthreshold_scap {
	int TempData[MAX_CHANNEL_NUM];
	int rawdata_test_max[MAX_CHANNEL_NUM];
	int rawdata_test_min[MAX_CHANNEL_NUM];
	int CiTest_Max[MAX_CHANNEL_NUM];
	int CiTest_Min[MAX_CHANNEL_NUM];
	int DeltaCiTest_Base[MAX_CHANNEL_NUM];
	int DeltaCiTest_AnotherBase1[MAX_CHANNEL_NUM];
	int DeltaCiTest_AnotherBase2[MAX_CHANNEL_NUM];
	int CiDeviationTest_Base[MAX_CHANNEL_NUM];

	int NoiseTest_Max[MAX_CHANNEL_NUM];
	int DeltaCxTest_Sort[MAX_CHANNEL_NUM];
	int DeltaCxTest_Area[MAX_CHANNEL_NUM];

	int cb_test_max[MAX_CHANNEL_NUM];
	int cb_test_min[MAX_CHANNEL_NUM];
	int DeltaCbTest_Base[MAX_CHANNEL_NUM];
	int DifferTest_Base[MAX_CHANNEL_NUM];
	int CBDeviationTest_Base[MAX_CHANNEL_NUM];
	int K1DifferTest_Base[MAX_CHANNEL_NUM];
};

void OnInit_InvalidNode(char *strIniFile);
void OnInit_DThreshold_RawDataTest(char *strIniFile);
void OnInit_DThreshold_SCapRawDataTest(char *strIniFile);
void OnInit_DThreshold_SCapCbTest(char *strIniFile);
void OnInit_DThreshold_ForceTouch_SCapRawDataTest(char *strIniFile);
void OnInit_DThreshold_ForceTouch_SCapCbTest(char *strIniFile);
void OnInit_DThreshold_RxLinearityTest(char *strIniFile);
void OnInit_DThreshold_TxLinearityTest(char *strIniFile);
void OnInit_DThreshold_PanelDifferTest(char *strIniFile);
void OnInit_DThreshold_CBTest(char *strIniFile);
void OnInit_DThreshold_AllButtonCBTest(char *strIniFile);
void OnThreshold_VkAndVaRawDataSeparateTest(char *strIniFile);
void OnGetTestItemParam(char *strItemName, char *strIniFile, int iDefautValue);
int malloc_struct_DetailThreshold(void);
void free_struct_DetailThreshold(void);
#endif
