/* packet-mtp2.c
 * Routines for MTP2 dissection
 * It is hopefully (needs testing) compliant to
 * ITU-T Q.703 and Q.703 Annex A.
 *
 * Copyright 2001, 2004 Michael Tuexen <tuexen [AT] fh-muenster.de>
 *
 * $Id: packet-mtp2.c 35224 2010-12-20 05:35:29Z guy $
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Copied from packet-m2pa.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <epan/packet.h>
#include <epan/prefs.h>
#include <epan/crc16.h>
#include <epan/expert.h>

/* Initialize the protocol and registered fields */
static int proto_mtp2        = -1;
static int hf_mtp2_bsn       = -1;
static int hf_mtp2_ext_bsn   = -1;
static int hf_mtp2_ext_res   = -1;
static int hf_mtp2_bib       = -1;
static int hf_mtp2_ext_bib   = -1;
static int hf_mtp2_fsn       = -1;
static int hf_mtp2_ext_fsn   = -1;
static int hf_mtp2_fib       = -1;
static int hf_mtp2_ext_fib   = -1;
static int hf_mtp2_li        = -1;
static int hf_mtp2_ext_li    = -1;
static int hf_mtp2_spare     = -1;
static int hf_mtp2_ext_spare = -1;
static int hf_mtp2_sf        = -1;
static int hf_mtp2_sf_extra  = -1;

/* Initialize the subtree pointers */
static gint ett_mtp2       = -1;

static dissector_handle_t mtp3_handle;
static gboolean use_extended_sequence_numbers_default = FALSE;
static gboolean use_extended_sequence_numbers         = FALSE;

#define BSN_BIB_LENGTH          1
#define FSN_FIB_LENGTH          1
#define LI_LENGTH               1
#define HEADER_LENGTH           (BSN_BIB_LENGTH + FSN_FIB_LENGTH + LI_LENGTH)

#define EXTENDED_BSN_BIB_LENGTH 2
#define EXTENDED_FSN_FIB_LENGTH 2
#define EXTENDED_LI_LENGTH      2
#define EXTENDED_HEADER_LENGTH  (EXTENDED_BSN_BIB_LENGTH + EXTENDED_FSN_FIB_LENGTH + EXTENDED_LI_LENGTH)

#define BSN_BIB_OFFSET          0
#define FSN_FIB_OFFSET          (BSN_BIB_OFFSET + BSN_BIB_LENGTH)
#define LI_OFFSET               (FSN_FIB_OFFSET + FSN_FIB_LENGTH)
#define SIO_OFFSET              (LI_OFFSET + LI_LENGTH)

#define EXTENDED_BSN_BIB_OFFSET 0
#define EXTENDED_FSN_FIB_OFFSET (EXTENDED_BSN_BIB_OFFSET + EXTENDED_BSN_BIB_LENGTH)
#define EXTENDED_LI_OFFSET      (EXTENDED_FSN_FIB_OFFSET + EXTENDED_FSN_FIB_LENGTH)
#define EXTENDED_SIO_OFFSET     (EXTENDED_LI_OFFSET + EXTENDED_LI_LENGTH)

#define BSN_MASK                0x7f
#define BIB_MASK                0x80
#define FSN_MASK                0x7f
#define FIB_MASK                0x80
#define LI_MASK                 0x3f
#define SPARE_MASK              0xc0

#define EXTENDED_BSN_MASK       0x0fff
#define EXTENDED_RES_MASK       0x7000
#define EXTENDED_BIB_MASK       0x8000
#define EXTENDED_FSN_MASK       0x0fff
#define EXTENDED_FIB_MASK       0x8000
#define EXTENDED_LI_MASK        0x01ff
#define EXTENDED_SPARE_MASK     0xfe00

static void
dissect_mtp2_header(tvbuff_t *su_tvb, proto_item *mtp2_tree)
{
  if (mtp2_tree) {
    if (use_extended_sequence_numbers) {
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_bsn,   su_tvb, EXTENDED_BSN_BIB_OFFSET, EXTENDED_BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_res,   su_tvb, EXTENDED_BSN_BIB_OFFSET, EXTENDED_BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_bib,   su_tvb, EXTENDED_BSN_BIB_OFFSET, EXTENDED_BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_fsn,   su_tvb, EXTENDED_FSN_FIB_OFFSET, EXTENDED_FSN_FIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_res,   su_tvb, EXTENDED_BSN_BIB_OFFSET, EXTENDED_BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_fib,   su_tvb, EXTENDED_FSN_FIB_OFFSET, EXTENDED_FSN_FIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_li,    su_tvb, EXTENDED_LI_OFFSET,      EXTENDED_LI_LENGTH,      ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_ext_spare, su_tvb, EXTENDED_LI_OFFSET,      EXTENDED_LI_LENGTH,      ENC_LITTLE_ENDIAN);
    } else {
      proto_tree_add_item(mtp2_tree, hf_mtp2_bsn,   su_tvb, BSN_BIB_OFFSET, BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_bib,   su_tvb, BSN_BIB_OFFSET, BSN_BIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_fsn,   su_tvb, FSN_FIB_OFFSET, FSN_FIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_fib,   su_tvb, FSN_FIB_OFFSET, FSN_FIB_LENGTH, ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_li,    su_tvb, LI_OFFSET,      LI_LENGTH,      ENC_LITTLE_ENDIAN);
      proto_tree_add_item(mtp2_tree, hf_mtp2_spare, su_tvb, LI_OFFSET,      LI_LENGTH,      ENC_LITTLE_ENDIAN);
    }
  }
}
/*
*******************************************************************************
* DETAILS : Calculate a new FCS-16 given the current FCS-16 and the new data.
*******************************************************************************
*/
static guint16
mtp2_fcs16(tvbuff_t * tvbuff)
{
    guint len = tvb_length(tvbuff)-2;

    /* Check for Invalid Length */
    if (len == 0)
        return (0x0000);
    return crc16_ccitt_tvb(tvbuff, len);
}

/*
 * This function for CRC16 only is based on the decode_fcs of packet_ppp.c
 */
static tvbuff_t *
mtp2_decode_crc16(tvbuff_t *tvb, proto_tree *fh_tree, packet_info *pinfo)
{
  tvbuff_t   *next_tvb;
  gint       len, reported_len;
  int        rx_fcs_offset;
  guint32    rx_fcs_exp;
  guint32    rx_fcs_got;
  int proto_offset=0;
  proto_item *cause;

  /*
   * Do we have the entire packet, and does it include a 2-byte FCS?
   */
  len = tvb_length_remaining(tvb, proto_offset);
  reported_len = tvb_reported_length_remaining(tvb, proto_offset);
  if (reported_len < 2 || len < 0) {
    /*
     * The packet is claimed not to even have enough data for a 2-byte FCS,
     * or we're already past the end of the captured data.
     * Don't slice anything off.
     */
    next_tvb = tvb_new_subset_remaining(tvb, proto_offset);
  } else if (len < reported_len) {
    /*
     * The packet is claimed to have enough data for a 2-byte FCS, but
     * we didn't capture all of the packet.
     * Slice off the 2-byte FCS from the reported length, and trim the
     * captured length so it's no more than the reported length; that
     * will slice off what of the FCS, if any, is in the captured
     * length.
     */
    reported_len -= 2;
    if (len > reported_len)
      len = reported_len;
    next_tvb = tvb_new_subset(tvb, proto_offset, len, reported_len);
  } else {
    /*
     * We have the entire packet, and it includes a 2-byte FCS.
     * Slice it off.
     */
    len -= 2;
    reported_len -= 2;
    next_tvb = tvb_new_subset(tvb, proto_offset, len, reported_len);
    
    /*
     * Compute the FCS and put it into the tree.
     */
    rx_fcs_offset = proto_offset + len;
    rx_fcs_exp = mtp2_fcs16(tvb);
    rx_fcs_got = tvb_get_letohs(tvb, rx_fcs_offset);
    if (rx_fcs_got != rx_fcs_exp) {
      cause=proto_tree_add_text(fh_tree, tvb, rx_fcs_offset, 2,
				"FCS 16: 0x%04x [incorrect, should be 0x%04x]",
				rx_fcs_got, rx_fcs_exp);
      proto_item_set_expert_flags(cause, PI_MALFORMED, PI_WARN);
      expert_add_info_format(pinfo, cause, PI_MALFORMED, PI_WARN, "MTP2 Frame CheckFCS 16 Error");
    } else {
      proto_tree_add_text(fh_tree, tvb, rx_fcs_offset, 2,
			  "FCS 16: 0x%04x [correct]",
			  rx_fcs_got);
    }
  }
  return next_tvb;
}


static void
dissect_mtp2_fisu(packet_info *pinfo)
{
  col_set_str(pinfo->cinfo, COL_INFO, "FISU ");
}

static const value_string status_field_vals[] = {
  { 0x0, "Status Indication O" },
  { 0x1, "Status Indication N" },
  { 0x2, "Status Indication E" },
  { 0x3, "Status Indication OS" },
  { 0x4, "Status Indication PO" },
  { 0x5, "Status Indication B" },
  { 0,   NULL}
};

/* Same as above but in acronym form (for the Info column) */
static const value_string status_field_acro_vals[] = {
  { 0x0, "SIO" },
  { 0x1, "SIN" },
  { 0x2, "SIE" },
  { 0x3, "SIOS" },
  { 0x4, "SIPO" },
  { 0x5, "SIB" },
  { 0,   NULL}
};

#define SF_OFFSET          (LI_OFFSET + LI_LENGTH)
#define EXTENDED_SF_OFFSET (EXTENDED_LI_OFFSET + EXTENDED_LI_LENGTH)

#define SF_LENGTH			1
#define SF_EXTRA_OFFSET			(SF_OFFSET + SF_LENGTH)
#define EXTENDED_SF_EXTRA_OFFSET	(EXTENDED_SF_OFFSET + SF_LENGTH)
#define SF_EXTRA_LENGTH			1

static void
dissect_mtp2_lssu(tvbuff_t *su_tvb, packet_info *pinfo, proto_item *mtp2_tree)
{
  guint8 sf = 0xFF;
  guint8 sf_offset, sf_extra_offset;
  
  if (use_extended_sequence_numbers) {
    sf_offset = EXTENDED_SF_OFFSET;
    sf_extra_offset = EXTENDED_SF_EXTRA_OFFSET;
  } else {
    sf_offset = SF_OFFSET;
    sf_extra_offset = SF_EXTRA_OFFSET;
  }

  proto_tree_add_item(mtp2_tree, hf_mtp2_sf, su_tvb, sf_offset, SF_LENGTH, ENC_LITTLE_ENDIAN);
  sf = tvb_get_guint8(su_tvb, SF_OFFSET);

  /*  If the LI is 2 then there is an extra octet following the standard SF
   *  field but it is not defined what this octet is.
   *  (In any case the first byte of the SF always has the same meaning.)
   */
  if ((tvb_get_guint8(su_tvb, LI_OFFSET) & LI_MASK) == 2)
    proto_tree_add_item(mtp2_tree, hf_mtp2_sf_extra, su_tvb, sf_extra_offset, SF_EXTRA_LENGTH, ENC_LITTLE_ENDIAN);

  if (check_col(pinfo->cinfo, COL_INFO))
    col_set_str(pinfo->cinfo, COL_INFO, val_to_str_const(sf, status_field_acro_vals, "Unknown"));
}

static void
dissect_mtp2_msu(tvbuff_t *su_tvb, packet_info *pinfo, proto_item *mtp2_item, proto_item *tree)
{
  gint sif_sio_length;
  tvbuff_t *sif_sio_tvb;

  col_set_str(pinfo->cinfo, COL_INFO, "MSU ");

  if (use_extended_sequence_numbers) {
    sif_sio_length = tvb_length(su_tvb) - EXTENDED_HEADER_LENGTH;
    sif_sio_tvb = tvb_new_subset(su_tvb, EXTENDED_SIO_OFFSET, sif_sio_length, sif_sio_length);
  } else {
    sif_sio_length = tvb_length(su_tvb) - HEADER_LENGTH;
    sif_sio_tvb = tvb_new_subset(su_tvb, SIO_OFFSET, sif_sio_length, sif_sio_length);
  }
  call_dissector(mtp3_handle, sif_sio_tvb, pinfo, tree);

  if (tree) {
    if (use_extended_sequence_numbers)
      proto_item_set_len(mtp2_item, EXTENDED_HEADER_LENGTH);
    else
      proto_item_set_len(mtp2_item, HEADER_LENGTH);
  }
}

static void
dissect_mtp2_su(tvbuff_t *su_tvb, packet_info *pinfo, proto_item *mtp2_item, proto_item *mtp2_tree, proto_tree *tree,gboolean validate_crc)
{
  guint16 li;
  tvbuff_t  *next_tvb = NULL;

  dissect_mtp2_header(su_tvb, mtp2_tree); 
  if (validate_crc)  
    next_tvb = mtp2_decode_crc16(su_tvb, mtp2_tree, pinfo);

  if (use_extended_sequence_numbers)
    li = tvb_get_letohs(su_tvb, EXTENDED_LI_OFFSET) & EXTENDED_LI_MASK;
  else
    li = tvb_get_guint8(su_tvb, LI_OFFSET) & LI_MASK;
  switch(li) {
  case 0:
    dissect_mtp2_fisu(pinfo);
    break;
  case 1:
  case 2: 
    if (validate_crc)  
      dissect_mtp2_lssu(next_tvb, pinfo, mtp2_tree);
    else
      dissect_mtp2_lssu(su_tvb, pinfo, mtp2_tree);
    break;
  default:
    /* In some capture files (like .rf5), CRC are not present */
    /* So, to avoid trouble, give the complete buffer if CRC validation is disabled */
    if (validate_crc)  
      dissect_mtp2_msu(next_tvb, pinfo, mtp2_item, tree);
    else 
      dissect_mtp2_msu(su_tvb, pinfo, mtp2_item, tree);
    break;
  }
}

static void
dissect_mtp2_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, gboolean validate_crc)
{
  proto_item *mtp2_item = NULL;
  proto_tree *mtp2_tree = NULL;

  if (pinfo->annex_a_used == MTP2_ANNEX_A_USED_UNKNOWN)
    use_extended_sequence_numbers = use_extended_sequence_numbers_default;
  else
    use_extended_sequence_numbers = (pinfo->annex_a_used == MTP2_ANNEX_A_USED);
    
  col_set_str(pinfo->cinfo, COL_PROTOCOL, "MTP2");
  
  if (tree) {
    mtp2_item = proto_tree_add_item(tree, proto_mtp2, tvb, 0, -1, ENC_NA);
    mtp2_tree = proto_item_add_subtree(mtp2_item, ett_mtp2);
  };

  dissect_mtp2_su(tvb, pinfo, mtp2_item, mtp2_tree, tree, validate_crc);
}

/* Dissect MTP2 frame with/without CRC16 included at end of payload */
static void
dissect_mtp2(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
  /* If the link extension indicates the FCS presence, then the Checkbits
   * have to be proceeded in the MTP2 dissector */
  if ( pinfo->fd->lnk_t == WTAP_ENCAP_ERF ) {
    dissect_mtp2_common(tvb, pinfo, tree, TRUE);
  } else {
    dissect_mtp2_common(tvb, pinfo, tree, FALSE);
  }
}

void
proto_register_mtp2(void)
{

  static hf_register_info hf[] = {
    { &hf_mtp2_bsn,       { "Backward sequence number", "mtp2.bsn",      FT_UINT8,  BASE_DEC, NULL,                    BSN_MASK,            NULL, HFILL } },
    { &hf_mtp2_ext_bsn,   { "Backward sequence number", "mtp2.bsn",      FT_UINT16, BASE_DEC, NULL,                    EXTENDED_BSN_MASK,   NULL, HFILL } },
    { &hf_mtp2_ext_res,   { "Reserved",                 "mtp2.res",      FT_UINT16, BASE_DEC, NULL,                    EXTENDED_RES_MASK,   NULL, HFILL } },
    { &hf_mtp2_bib,       { "Backward indicator bit",   "mtp2.bib",      FT_UINT8,  BASE_DEC, NULL,                    BIB_MASK,            NULL, HFILL } },
    { &hf_mtp2_ext_bib,   { "Backward indicator bit",   "mtp2.bib",      FT_UINT16, BASE_DEC, NULL,                    EXTENDED_BIB_MASK,   NULL, HFILL } },
    { &hf_mtp2_fsn,       { "Forward sequence number",  "mtp2.fsn",      FT_UINT8,  BASE_DEC, NULL,                    FSN_MASK,            NULL, HFILL } },
    { &hf_mtp2_ext_fsn,   { "Forward sequence number",  "mtp2.fsn",      FT_UINT16, BASE_DEC, NULL,                    EXTENDED_FSN_MASK,   NULL, HFILL } },
    { &hf_mtp2_fib,       { "Forward indicator bit",    "mtp2.fib",      FT_UINT8,  BASE_DEC, NULL,                    FIB_MASK,            NULL, HFILL } },
    { &hf_mtp2_ext_fib,   { "Forward indicator bit",    "mtp2.fib",      FT_UINT16, BASE_DEC, NULL,                    EXTENDED_FIB_MASK,   NULL, HFILL } },
    { &hf_mtp2_li,        { "Length Indicator",         "mtp2.li",       FT_UINT8,  BASE_DEC, NULL,                    LI_MASK,             NULL, HFILL } },
    { &hf_mtp2_ext_li,    { "Length Indicator",         "mtp2.li",       FT_UINT16, BASE_DEC, NULL,                    EXTENDED_LI_MASK,    NULL, HFILL } },
    { &hf_mtp2_spare,     { "Spare",                    "mtp2.spare",    FT_UINT8,  BASE_DEC, NULL,                    SPARE_MASK,          NULL, HFILL } },
    { &hf_mtp2_ext_spare, { "Spare",                    "mtp2.spare",    FT_UINT16, BASE_DEC, NULL,                    EXTENDED_SPARE_MASK, NULL, HFILL } },
    { &hf_mtp2_sf,        { "Status field",             "mtp2.sf",       FT_UINT8,  BASE_DEC, VALS(status_field_vals), 0x0,                 NULL, HFILL } },
    { &hf_mtp2_sf_extra,  { "Status field extra octet", "mtp2.sf_extra", FT_UINT8,  BASE_HEX, NULL,                    0x0,                 NULL, HFILL } }
  };

  static gint *ett[] = {
    &ett_mtp2
  };

  module_t *mtp2_module;

  proto_mtp2 = proto_register_protocol("Message Transfer Part Level 2", "MTP2", "mtp2");
  register_dissector("mtp2", dissect_mtp2, proto_mtp2);

  proto_register_field_array(proto_mtp2, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
  
  mtp2_module = prefs_register_protocol(proto_mtp2, NULL);
  prefs_register_bool_preference(mtp2_module, 
                                 "use_extended_sequence_numbers",
                                 "Use extended sequence numbers",
                                 "Whether the MTP2 dissector should use extended sequence numbers as described in Q.703, Annex A as a default.",
                                 &use_extended_sequence_numbers_default);


}

void
proto_reg_handoff_mtp2(void)
{
  dissector_handle_t mtp2_handle;

  mtp2_handle = find_dissector("mtp2");
  dissector_add_uint("wtap_encap", WTAP_ENCAP_MTP2, mtp2_handle);
  dissector_add_uint("wtap_encap", WTAP_ENCAP_MTP2_WITH_PHDR, mtp2_handle);

  mtp3_handle   = find_dissector("mtp3");
}
