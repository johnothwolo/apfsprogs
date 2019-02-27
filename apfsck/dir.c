/*
 *  apfsprogs/apfsck/dir.c
 *
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include "apfsck.h"
#include "dir.h"
#include "inode.h"
#include "key.h"
#include "super.h"

/**
 * read_sibling_id_xfield - Parse a sibling id xfield and check its consistency
 * @xval:	pointer to the xfield value
 * @len:	remaining length of the dentry value
 * @sibling_id:	on return, the sibling id
 *
 * Returns the length of the xfield value.
 */
static int read_sibling_id_xfield(char *xval, int len, u64 *sibling_id)
{
	__le64 *id_raw;

	if (len < 8)
		report("Sibling link xfield", "doesn't fit in dentry record.");
	id_raw = (__le64 *)xval;

	*sibling_id = le64_to_cpu(*id_raw);

	return sizeof(*id_raw);
}

/**
 * parse_dentry_xfields - Parse and check a dentry extended fields
 * @xblob:	pointer to the beginning of the xfields in the dentry value
 * @len:	length of the xfields
 * @sibling_id: on return, the sibling id (0 if none)
 *
 * Internal consistency of @key must be checked before calling this function.
 */
static void parse_dentry_xfields(struct apfs_xf_blob *xblob, int len,
				 u64 *sibling_id)
{
	struct apfs_x_field *xfield;
	char *xval;
	int xcount;
	int i;

	*sibling_id = 0;
	if (len == 0) /* No extended fields */
		return;

	len -= sizeof(*xblob);
	if (len < 0)
		report("Dentry record", "no room for extended fields.");
	xcount = le16_to_cpu(xblob->xf_num_exts);

	xfield = (struct apfs_x_field *)xblob->xf_data;
	xval = (char *)xfield + xcount * sizeof(xfield[0]);
	len -= xcount * sizeof(xfield[0]);
	if (len < 0)
		report("Dentry record", "number of xfields cannot fit.");

	/* The official reference seems to be wrong here */
	if (le16_to_cpu(xblob->xf_used_data) != len)
		report("Dentry record",
		       "value size incompatible with xfields.");

	/* TODO: could a dentry actually have more than one xfield? */
	for (i = 0; i < le16_to_cpu(xblob->xf_num_exts); ++i) {
		int xlen, xpad_len;

		switch (xfield[i].x_type) {
		case APFS_DREC_EXT_TYPE_SIBLING_ID:
			xlen = read_sibling_id_xfield(xval, len, sibling_id);
			break;
		default:
			report("Dentry xfield", "invalid type.");
		}

		if (xlen != le16_to_cpu(xfield[i].x_size))
			report("Dentry xfield", "wrong size");
		len -= xlen;
		xval += xlen;

		/* Attribute length is padded with zeroes to a multiple of 8 */
		xpad_len = ROUND_UP(xlen, 8) - xlen;
		len -= xpad_len;
		if (len < 0)
			report("Dentry xfield", "doesn't fit in record value.");

		for (; xpad_len; ++xval, --xpad_len)
			if (*xval)
				report("Dentry xfield", "non-zero padding.");
	}

	if (len)
		report("Dentry record", "length of xfields does not add up.");
}

/**
 * parse_dentry_record - Parse a dentry record value and check for corruption
 * @key:	pointer to the raw key
 * @val:	pointer to the raw value
 * @len:	length of the raw value
 *
 * Internal consistency of @key must be checked before calling this function.
 */
void parse_dentry_record(struct apfs_drec_hashed_key *key,
			 struct apfs_drec_val *val, int len)
{
	u64 ino, parent_ino;
	struct inode *inode, *parent;
	int namelen = le32_to_cpu(key->name_len_and_hash) & 0x3FFU;
	u16 filetype, dtype;
	u64 sibling_id;
	struct sibling *sibling;

	if (len < sizeof(*val))
		report("Dentry record", "value is too small.");

	ino = le64_to_cpu(val->file_id);
	inode = get_inode(ino, vsb->v_inode_table);
	inode->i_link_count++;

	parent_ino = cat_cnid(&key->hdr);
	check_inode_ids(ino, parent_ino);
	if (parent_ino != APFS_ROOT_DIR_PARENT) {
		parent = get_inode(parent_ino, vsb->v_inode_table);
		if (!parent->i_seen) /* The b-tree keys are in order */
			report("Dentry record", "parent inode missing");
		if ((parent->i_mode & S_IFMT) != S_IFDIR)
			report("Dentry record", "parent inode not directory.");
		parent->i_child_count++;
	}

	dtype = le16_to_cpu(val->flags) & APFS_DREC_TYPE_MASK;
	if (dtype != le16_to_cpu(val->flags))
		report("Dentry record", "reserved flags in use.");

	/* The mode may have already been set by the inode or another dentry */
	filetype = inode->i_mode >> 12;
	if (filetype && filetype != dtype)
		report("Dentry record", "file mode doesn't match dentry type.");
	if (dtype == 0) /* Don't save a 0, that means the mode is not set */
		report("Dentry record", "invalid dentry type.");
	inode->i_mode |= dtype << 12;

	parse_dentry_xfields((struct apfs_xf_blob *)val->xfields,
			     len - sizeof(*val), &sibling_id);

	if (!sibling_id) /* No sibling record for this dentry */
		return;
	sibling = get_sibling(sibling_id, namelen, inode);
	set_or_check_sibling(parent_ino, namelen, key->name, sibling);
}
