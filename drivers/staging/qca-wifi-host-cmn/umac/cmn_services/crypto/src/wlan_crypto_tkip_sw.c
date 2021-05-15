/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
 */
/*
 * Temporal Key Integrity Protocol (CCMP)
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <qdf_types.h>
#include <qdf_mem.h>
#include <qdf_util.h>
#include "wlan_crypto_aes_i.h"
#include "wlan_crypto_def_i.h"


static inline uint16_t RotR1(uint16_t val)
{
	return (val >> 1) | (val << 15);
}


static inline uint8_t Lo8(uint16_t val)
{
	return val & 0xff;
}


static inline uint8_t Hi8(uint16_t val)
{
	return val >> 8;
}


static inline uint16_t Lo16(uint32_t val)
{
	return val & 0xffff;
}


static inline uint16_t Hi16(uint32_t val)
{
	return val >> 16;
}


static inline uint16_t Mk16(uint8_t hi, uint8_t lo)
{
	return lo | (((u16) hi) << 8);
}


static inline uint16_t Mk16_le(uint16_t *v)
{
	return qdf_le16_to_cpu(*v);
}


static const uint16_t Sbox[256] = {
	0xC6A5, 0xF884, 0xEE99, 0xF68D, 0xFF0D, 0xD6BD, 0xDEB1, 0x9154,
	0x6050, 0x0203, 0xCEA9, 0x567D, 0xE719, 0xB562, 0x4DE6, 0xEC9A,
	0x8F45, 0x1F9D, 0x8940, 0xFA87, 0xEF15, 0xB2EB, 0x8EC9, 0xFB0B,
	0x41EC, 0xB367, 0x5FFD, 0x45EA, 0x23BF, 0x53F7, 0xE496, 0x9B5B,
	0x75C2, 0xE11C, 0x3DAE, 0x4C6A, 0x6C5A, 0x7E41, 0xF502, 0x834F,
	0x685C, 0x51F4, 0xD134, 0xF908, 0xE293, 0xAB73, 0x6253, 0x2A3F,
	0x080C, 0x9552, 0x4665, 0x9D5E, 0x3028, 0x37A1, 0x0A0F, 0x2FB5,
	0x0E09, 0x2436, 0x1B9B, 0xDF3D, 0xCD26, 0x4E69, 0x7FCD, 0xEA9F,
	0x121B, 0x1D9E, 0x5874, 0x342E, 0x362D, 0xDCB2, 0xB4EE, 0x5BFB,
	0xA4F6, 0x764D, 0xB761, 0x7DCE, 0x527B, 0xDD3E, 0x5E71, 0x1397,
	0xA6F5, 0xB968, 0x0000, 0xC12C, 0x4060, 0xE31F, 0x79C8, 0xB6ED,
	0xD4BE, 0x8D46, 0x67D9, 0x724B, 0x94DE, 0x98D4, 0xB0E8, 0x854A,
	0xBB6B, 0xC52A, 0x4FE5, 0xED16, 0x86C5, 0x9AD7, 0x6655, 0x1194,
	0x8ACF, 0xE910, 0x0406, 0xFE81, 0xA0F0, 0x7844, 0x25BA, 0x4BE3,
	0xA2F3, 0x5DFE, 0x80C0, 0x058A, 0x3FAD, 0x21BC, 0x7048, 0xF104,
	0x63DF, 0x77C1, 0xAF75, 0x4263, 0x2030, 0xE51A, 0xFD0E, 0xBF6D,
	0x814C, 0x1814, 0x2635, 0xC32F, 0xBEE1, 0x35A2, 0x88CC, 0x2E39,
	0x9357, 0x55F2, 0xFC82, 0x7A47, 0xC8AC, 0xBAE7, 0x322B, 0xE695,
	0xC0A0, 0x1998, 0x9ED1, 0xA37F, 0x4466, 0x547E, 0x3BAB, 0x0B83,
	0x8CCA, 0xC729, 0x6BD3, 0x283C, 0xA779, 0xBCE2, 0x161D, 0xAD76,
	0xDB3B, 0x6456, 0x744E, 0x141E, 0x92DB, 0x0C0A, 0x486C, 0xB8E4,
	0x9F5D, 0xBD6E, 0x43EF, 0xC4A6, 0x39A8, 0x31A4, 0xD337, 0xF28B,
	0xD532, 0x8B43, 0x6E59, 0xDAB7, 0x018C, 0xB164, 0x9CD2, 0x49E0,
	0xD8B4, 0xACFA, 0xF307, 0xCF25, 0xCAAF, 0xF48E, 0x47E9, 0x1018,
	0x6FD5, 0xF088, 0x4A6F, 0x5C72, 0x3824, 0x57F1, 0x73C7, 0x9751,
	0xCB23, 0xA17C, 0xE89C, 0x3E21, 0x96DD, 0x61DC, 0x0D86, 0x0F85,
	0xE090, 0x7C42, 0x71C4, 0xCCAA, 0x90D8, 0x0605, 0xF701, 0x1C12,
	0xC2A3, 0x6A5F, 0xAEF9, 0x69D0, 0x1791, 0x9958, 0x3A27, 0x27B9,
	0xD938, 0xEB13, 0x2BB3, 0x2233, 0xD2BB, 0xA970, 0x0789, 0x33A7,
	0x2DB6, 0x3C22, 0x1592, 0xC920, 0x8749, 0xAAFF, 0x5078, 0xA57A,
	0x038F, 0x59F8, 0x0980, 0x1A17, 0x65DA, 0xD731, 0x84C6, 0xD0B8,
	0x82C3, 0x29B0, 0x5A77, 0x1E11, 0x7BCB, 0xA8FC, 0x6DD6, 0x2C3A,
};


static inline uint16_t _S_(uint16_t v)
{
	uint16_t t = Sbox[Hi8(v)];
	return Sbox[Lo8(v)] ^ ((t << 8) | (t >> 8));
}


#define PHASE1_LOOP_COUNT 8

static void tkip_mixing_phase1(uint16_t *TTAK, const uint8_t *TK,
				const uint8_t *TA, uint32_t IV32)
{
	int i, j;

	/* Initialize the 80-bit TTAK from TSC (IV32) and TA[0..5] */
	TTAK[0] = Lo16(IV32);
	TTAK[1] = Hi16(IV32);
	TTAK[2] = Mk16(TA[1], TA[0]);
	TTAK[3] = Mk16(TA[3], TA[2]);
	TTAK[4] = Mk16(TA[5], TA[4]);

	for (i = 0; i < PHASE1_LOOP_COUNT; i++) {
		j = 2 * (i & 1);
		TTAK[0] += _S_(TTAK[4] ^ Mk16(TK[1 + j], TK[0 + j]));
		TTAK[1] += _S_(TTAK[0] ^ Mk16(TK[5 + j], TK[4 + j]));
		TTAK[2] += _S_(TTAK[1] ^ Mk16(TK[9 + j], TK[8 + j]));
		TTAK[3] += _S_(TTAK[2] ^ Mk16(TK[13 + j], TK[12 + j]));
		TTAK[4] += _S_(TTAK[3] ^ Mk16(TK[1 + j], TK[0 + j])) + i;
	}
}


static void tkip_mixing_phase2(uint8_t *WEPSeed, const uint8_t *TK,
				const uint16_t *TTAK, uint16_t IV16){
	uint16_t PPK[6];

	/* Step 1 - make copy of TTAK and bring in TSC */
	PPK[0] = TTAK[0];
	PPK[1] = TTAK[1];
	PPK[2] = TTAK[2];
	PPK[3] = TTAK[3];
	PPK[4] = TTAK[4];
	PPK[5] = TTAK[4] + IV16;

	/* Step 2 - 96-bit bijective mixing using S-box */
	PPK[0] += _S_(PPK[5] ^ Mk16_le((uint16_t *) &TK[0]));
	PPK[1] += _S_(PPK[0] ^ Mk16_le((uint16_t *) &TK[2]));
	PPK[2] += _S_(PPK[1] ^ Mk16_le((uint16_t *) &TK[4]));
	PPK[3] += _S_(PPK[2] ^ Mk16_le((uint16_t *) &TK[6]));
	PPK[4] += _S_(PPK[3] ^ Mk16_le((uint16_t *) &TK[8]));
	PPK[5] += _S_(PPK[4] ^ Mk16_le((uint16_t *) &TK[10]));

	PPK[0] += RotR1(PPK[5] ^ Mk16_le((uint16_t *) &TK[12]));
	PPK[1] += RotR1(PPK[0] ^ Mk16_le((uint16_t *) &TK[14]));
	PPK[2] += RotR1(PPK[1]);
	PPK[3] += RotR1(PPK[2]);
	PPK[4] += RotR1(PPK[3]);
	PPK[5] += RotR1(PPK[4]);

	/* Step 3 - bring in last of TK bits, assign 24-bit WEP IV value
	 * WEPSeed[0..2] is transmitted as WEP IV */
	WEPSeed[0] = Hi8(IV16);
	WEPSeed[1] = (Hi8(IV16) | 0x20) & 0x7F;
	WEPSeed[2] = Lo8(IV16);
	WEPSeed[3] = Lo8((PPK[5] ^ Mk16_le((uint16_t *) &TK[0])) >> 1);
	wlan_crypto_put_le16(&WEPSeed[4], PPK[0]);
	wlan_crypto_put_le16(&WEPSeed[6], PPK[1]);
	wlan_crypto_put_le16(&WEPSeed[8], PPK[2]);
	wlan_crypto_put_le16(&WEPSeed[10], PPK[3]);
	wlan_crypto_put_le16(&WEPSeed[12], PPK[4]);
	wlan_crypto_put_le16(&WEPSeed[14], PPK[5]);
}


static inline uint32_t rotl(uint32_t val, int bits)
{
	return (val << bits) | (val >> (32 - bits));
}

static inline uint32_t xswap(uint32_t val)
{
	return ((val & 0x00ff00ff) << 8) | ((val & 0xff00ff00) >> 8);
}


#define michael_block(l, r)	\
do {				\
	r ^= rotl(l, 17);	\
	l += r;			\
	r ^= xswap(l);		\
	l += r;			\
	r ^= rotl(l, 3);	\
	l += r;			\
	r ^= rotr(l, 2);	\
	l += r;			\
} while (0)


static void michael_mic(const uint8_t *key, const uint8_t *hdr,
			const uint8_t *data, size_t data_len, uint8_t *mic){
	uint32_t l, r;
	int i, blocks, last;

	l = wlan_crypto_get_le32(key);
	r = wlan_crypto_get_le32(key + 4);

	/* Michael MIC pseudo header: DA, SA, 3 x 0, Priority */
	l ^= wlan_crypto_get_le32(hdr);
	michael_block(l, r);
	l ^= wlan_crypto_get_le32(&hdr[4]);
	michael_block(l, r);
	l ^= wlan_crypto_get_le32(&hdr[8]);
	michael_block(l, r);
	l ^= wlan_crypto_get_le32(&hdr[12]);
	michael_block(l, r);

	/* 32-bit blocks of data */
	blocks = data_len / 4;
	last = data_len % 4;
	for (i = 0; i < blocks; i++) {
		l ^= wlan_crypto_get_le32(&data[4 * i]);
		michael_block(l, r);
	}

	/* Last block and padding (0x5a, 4..7 x 0) */
	switch (last) {
	case 0:
		l ^= 0x5a;
		break;
	case 1:
		l ^= data[4 * i] | 0x5a00;
		break;
	case 2:
		l ^= data[4 * i] | (data[4 * i + 1] << 8) | 0x5a0000;
		break;
	case 3:
		l ^= data[4 * i] | (data[4 * i + 1] << 8) |
			(data[4 * i + 2] << 16) | 0x5a000000;
		break;
	}
	michael_block(l, r);
	/* l ^= 0; */
	michael_block(l, r);

	wlan_crypto_put_le32(mic, l);
	wlan_crypto_put_le32(mic + 4, r);
}


static void michael_mic_hdr(const struct ieee80211_hdr *hdr11, uint8_t *hdr)
{
	int hdrlen = 24;

	switch (hdr11->frame_control[1] & (WLAN_FC1_FROMDS | WLAN_FC1_TODS)) {
	case WLAN_FC1_TODS:
		qdf_mem_copy(hdr, hdr11->addr3, WLAN_ALEN); /* DA */
		qdf_mem_copy(hdr + WLAN_ALEN, hdr11->addr2, WLAN_ALEN); /* SA */
		break;
	case WLAN_FC1_FROMDS:
		qdf_mem_copy(hdr, hdr11->addr1, WLAN_ALEN); /* DA */
		qdf_mem_copy(hdr + WLAN_ALEN, hdr11->addr3, WLAN_ALEN); /* SA */
		break;
	case WLAN_FC1_FROMDS | WLAN_FC1_TODS:
		qdf_mem_copy(hdr, hdr11->addr3, WLAN_ALEN); /* DA */
		qdf_mem_copy(hdr + WLAN_ALEN, hdr11 + 1, WLAN_ALEN); /* SA */
		hdrlen += WLAN_ALEN;
		break;
	case 0:
		qdf_mem_copy(hdr, hdr11->addr1, WLAN_ALEN); /* DA */
		qdf_mem_copy(hdr + WLAN_ALEN, hdr11->addr2, WLAN_ALEN); /* SA */
		break;
	}

	if (WLAN_FC0_GET_TYPE(hdr11->frame_control[0]) == WLAN_FC0_TYPE_DATA &&
	    (WLAN_FC0_GET_STYPE(hdr11->frame_control[0]) & 0x08)) {
		const uint8_t *qos = ((const uint8_t *) hdr11) + hdrlen;
		hdr[12] = qos[0] & 0x0f; /* priority */
	} else
		hdr[12] = 0; /* priority */

	hdr[13] = hdr[14] = hdr[15] = 0; /* reserved */
}


uint8_t *wlan_crypto_tkip_decrypt(const uint8_t *tk,
					const struct ieee80211_hdr *hdr,
					uint8_t *data, size_t data_len){
	uint16_t iv16;
	uint32_t iv32;
	uint16_t ttak[5];
	uint8_t rc4key[16];
	uint8_t *plain;
	size_t plain_len;
	uint32_t icv, rx_icv;
	const uint8_t *mic_key;
	uint8_t michael_hdr[16];
	uint8_t mic[8];

	if (data_len < 8 + 4)
		return NULL;

	iv16 = (data[0] << 8) | data[2];
	iv32 = wlan_crypto_get_le32(&data[4]);
	wpa_printf(MSG_EXCESSIVE, "TKIP decrypt: iv32=%08x iv16=%04x",
		   iv32, iv16);

	tkip_mixing_phase1(ttak, tk, hdr->addr2, iv32);
	wpa_hexdump(MSG_EXCESSIVE, "TKIP TTAK", (uint8_t *) ttak, sizeof(ttak));
	tkip_mixing_phase2(rc4key, tk, ttak, iv16);
	wpa_hexdump(MSG_EXCESSIVE, "TKIP RC4KEY", rc4key, sizeof(rc4key));

	plain_len = data_len - 8;
	plain = data + 8;
	wlan_crypto_wep_crypt(rc4key, plain, plain_len);

	icv = wlan_crypto_crc32(plain, plain_len - 4);
	rx_icv = wlan_crypto_get_le32(plain + plain_len - 4);
	if (icv != rx_icv) {
		wpa_printf(MSG_INFO, "TKIP ICV mismatch in frame from " MACSTR,
			   MAC2STR(hdr->addr2));
		wpa_printf(MSG_DEBUG, "TKIP calculated ICV %08x  received ICV "
			   "%08x", icv, rx_icv);
		qdf_mem_free(plain);
		return NULL;
	}
	plain_len -= 4;

	/* TODO: MSDU reassembly */

	if (plain_len < 8) {
		wpa_printf(MSG_INFO, "TKIP: Not enough room for Michael MIC "
			   "in a frame from " MACSTR, MAC2STR(hdr->addr2));
		qdf_mem_free(plain);
		return NULL;
	}

	michael_mic_hdr(hdr, michael_hdr);
	mic_key = tk + ((hdr->frame_control[1] & WLAN_FC1_FROMDS) ? 16 : 24);
	michael_mic(mic_key, michael_hdr, plain, plain_len - 8, mic);
	if (qdf_mem_cmp(mic, plain + plain_len - 8, 8) != 0) {
		wpa_printf(MSG_INFO, "TKIP: Michael MIC mismatch in a frame "
			   "from " MACSTR, MAC2STR(hdr->addr2));
		wpa_hexdump(MSG_DEBUG, "TKIP: Calculated MIC", mic, 8);
		wpa_hexdump(MSG_DEBUG, "TKIP: Received MIC",
			    plain + plain_len - 8, 8);
		return NULL;
	}

	return data;
}


void tkip_get_pn(uint8_t *pn, const uint8_t *data)
{
	pn[0] = data[7]; /* PN5 */
	pn[1] = data[6]; /* PN4 */
	pn[2] = data[5]; /* PN3 */
	pn[3] = data[4]; /* PN2 */
	pn[4] = data[0]; /* PN1 */
	pn[5] = data[2]; /* PN0 */
}


uint8_t *wlan_crypto_tkip_encrypt(const uint8_t *tk, uint8_t *frame,
				size_t len, size_t hdrlen){
	uint8_t michael_hdr[16];
	uint8_t mic[8];
	struct ieee80211_hdr *hdr;
	const uint8_t *mic_key;
	uint8_t *pos;
	uint16_t iv16;
	uint32_t iv32;
	uint16_t ttak[5];
	uint8_t rc4key[16];

	if (len < sizeof(*hdr) || len < hdrlen)
		return NULL;
	hdr = (struct ieee80211_hdr *) frame;

	michael_mic_hdr(hdr, michael_hdr);
	mic_key = tk + ((hdr->frame_control[1] & WLAN_FC1_FROMDS) ? 16 : 24);
	michael_mic(mic_key, michael_hdr, frame + hdrlen, len - hdrlen, mic);
	wpa_hexdump(MSG_EXCESSIVE, "TKIP: MIC", mic, sizeof(mic));
	pos = frame + hdrlen;

	iv32 = wlan_crypto_get_be32(pos);
	iv16 = wlan_crypto_get_be16(pos + 4);
	tkip_mixing_phase1(ttak, tk, hdr->addr2, iv32);
	wpa_hexdump(MSG_EXCESSIVE, "TKIP TTAK", (uint8_t *) ttak, sizeof(ttak));
	tkip_mixing_phase2(rc4key, tk, ttak, iv16);
	wpa_hexdump(MSG_EXCESSIVE, "TKIP RC4KEY", rc4key, sizeof(rc4key));

	qdf_mem_copy(pos, rc4key, 3);
	pos += 8;

	qdf_mem_copy(pos + len - hdrlen, mic, sizeof(mic));
	wlan_crypto_put_le32(pos + len - hdrlen + sizeof(mic),
		     wlan_crypto_crc32(pos, len - hdrlen + sizeof(mic)));
	wlan_crypto_wep_crypt(rc4key, pos, len - hdrlen + sizeof(mic) + 4);

	return frame;
}
