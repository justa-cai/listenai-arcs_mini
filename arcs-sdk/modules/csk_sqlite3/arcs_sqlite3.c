/* From: https://chromium.googlesource.com/chromium/src.git/+/4.1.249.1050/third_party/sqlite/src/os_symbian.cc
 * https://github.com/spsoft/spmemvfs/tree/master/spmemvfs
 * http://www.sqlite.org/src/doc/trunk/src/test_demovfs.c
 * http://www.sqlite.org/src/doc/trunk/src/test_vfstrace.c
 * http://www.sqlite.org/src/doc/trunk/src/test_onefile.c
 * http://www.sqlite.org/src/doc/trunk/src/test_vfs.c
 * https://github.com/nodemcu/nodemcu-firmware/blob/master/app/sqlite3/esp8266.c
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
// #include <unistd.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include "shox96_0_2.h"
#include <lsfs.h>
#include "sqlite3_malloc.c"

#define UNUSED(x) (void)(x)

#undef dbg_printf
// #define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_printf(...)
#define CACHEBLOCKSZ 64
#define csk_DEFAULT_MAXNAMESIZE 100

// From https://stackoverflow.com/questions/19758270/read-varint-from-linux-sockets#19760246
// Encode an unsigned 64-bit varint.  Returns number of encoded bytes.
// 'buffer' must have room for up to 10 bytes.
int encode_unsigned_varint(uint8_t *buffer, uint64_t value)
{
	int encoded = 0;
	do {
		uint8_t next_byte = value & 0x7F;
		value >>= 7;
		if (value)
			next_byte |= 0x80;
		buffer[encoded++] = next_byte;
	} while (value);
	return encoded;
}

uint64_t decode_unsigned_varint(const uint8_t *data, int *decoded_bytes)
{
	int i = 0;
	uint64_t decoded_value = 0;
	int shift_amount = 0;
	do {
		decoded_value |= (uint64_t)(data[i] & 0x7F) << shift_amount;
		shift_amount += 7;
	} while ((data[i++] & 0x80) != 0);
	*decoded_bytes = i;
	return decoded_value;
}

int csk_Close(sqlite3_file *);
int csk_Lock(sqlite3_file *, int);
int csk_Unlock(sqlite3_file *, int);
int csk_Sync(sqlite3_file *, int);
int csk_Open(sqlite3_vfs *, const char *, sqlite3_file *, int, int *);
int csk_Read(sqlite3_file *, void *, int, sqlite3_int64);
int csk_Write(sqlite3_file *, const void *, int, sqlite3_int64);
int csk_Truncate(sqlite3_file *, sqlite3_int64);
int csk_Delete(sqlite3_vfs *, const char *, int);
int csk_FileSize(sqlite3_file *, sqlite3_int64 *);
int csk_Access(sqlite3_vfs *, const char *, int, int *);
int csk_FullPathname(sqlite3_vfs *, const char *, int, char *);
int csk_CheckReservedLock(sqlite3_file *, int *);
int csk_FileControl(sqlite3_file *, int, void *);
int csk_SectorSize(sqlite3_file *);
int csk_DeviceCharacteristics(sqlite3_file *);
void *csk_DlOpen(sqlite3_vfs *, const char *);
void csk_DlError(sqlite3_vfs *, int, char *);
void (*csk_DlSym(sqlite3_vfs *, void *, const char *))(void);
void csk_DlClose(sqlite3_vfs *, void *);
int csk_Randomness(sqlite3_vfs *, int, char *);
int csk_Sleep(sqlite3_vfs *, int);
int csk_CurrentTime(sqlite3_vfs *, double *);

int cskmem_Close(sqlite3_file *);
int cskmem_Read(sqlite3_file *, void *, int, sqlite3_int64);
int cskmem_Write(sqlite3_file *, const void *, int, sqlite3_int64);
int cskmem_FileSize(sqlite3_file *, sqlite3_int64 *);
int cskmem_Sync(sqlite3_file *, int);

typedef struct st_linkedlist {
	uint16_t blockid;
	struct st_linkedlist *next;
	uint8_t data[CACHEBLOCKSZ];
} linkedlist_t, *pLinkedList_t;

typedef struct st_filecache {
	uint32_t size;
	linkedlist_t *list;
} filecache_t, *pFileCache_t;

typedef struct csk_file {
	sqlite3_file base;
	struct lsfs_file_t *fd;
	filecache_t *cache;
	char name[csk_DEFAULT_MAXNAMESIZE];
} csk_file;

sqlite3_vfs cskVfs = {
	1, // iVersion
	sizeof(csk_file), // szOsFile
	101, // mxPathname
	NULL, // pNext
	"csk", // name
	0, // pAppData
	csk_Open, // xOpen
	csk_Delete, // xDelete
	csk_Access, // xAccess
	csk_FullPathname, // xFullPathname
	csk_DlOpen, // xDlOpen
	csk_DlError, // xDlError
	csk_DlSym, // xDlSym
	csk_DlClose, // xDlClose
	csk_Randomness, // xRandomness
	csk_Sleep, // xSleep
	csk_CurrentTime, // xCurrentTime
	0 // xGetLastError
};

const sqlite3_io_methods cskIoMethods = { 1,
					  csk_Close,
					  csk_Read,
					  csk_Write,
					  csk_Truncate,
					  csk_Sync,
					  csk_FileSize,
					  csk_Lock,
					  csk_Unlock,
					  csk_CheckReservedLock,
					  csk_FileControl,
					  csk_SectorSize,
					  csk_DeviceCharacteristics };

const sqlite3_io_methods cskMemMethods = { 1,
					   cskmem_Close,
					   cskmem_Read,
					   cskmem_Write,
					   csk_Truncate,
					   cskmem_Sync,
					   cskmem_FileSize,
					   csk_Lock,
					   csk_Unlock,
					   csk_CheckReservedLock,
					   csk_FileControl,
					   csk_SectorSize,
					   csk_DeviceCharacteristics };

uint32_t linkedlist_store(linkedlist_t **leaf, uint32_t offset, uint32_t len, const uint8_t *data)
{
	const uint8_t blank[CACHEBLOCKSZ] = { 0 };
	uint16_t blockid = offset / CACHEBLOCKSZ;
	linkedlist_t *block;

	if (!memcmp(data, blank, CACHEBLOCKSZ))
		return len;

	block = *leaf;
	if (!block || (block->blockid != blockid)) {
		block = (linkedlist_t *)sqlite3_malloc(sizeof(linkedlist_t));
		if (!block)
			return SQLITE_NOMEM;

		memset(block->data, 0, CACHEBLOCKSZ);
		block->blockid = blockid;
	}

	if (!*leaf) {
		*leaf = block;
		block->next = NULL;
	} else if (block != *leaf) {
		if (block->blockid > (*leaf)->blockid) {
			block->next = (*leaf)->next;
			(*leaf)->next = block;
		} else {
			block->next = (*leaf);
			(*leaf) = block;
		}
	}

	memcpy(block->data + offset % CACHEBLOCKSZ, data, len);

	return len;
}

uint32_t filecache_pull(pFileCache_t cache, uint32_t offset, uint32_t len, uint8_t *data)
{
	uint16_t i;
	float blocks;
	uint32_t r = 0;

	blocks = (offset % CACHEBLOCKSZ + len) / (float)CACHEBLOCKSZ;
	if (blocks == 0.0)
		return 0;
	if (!cache->list)
		return 0;

	if ((blocks - (int)blocks) > 0.0)
		blocks = blocks + 1.0;

	for (i = 0; i < (uint16_t)blocks; i++) {
		uint16_t round;
		float relablock;
		linkedlist_t *leaf;
		uint32_t relaoffset, relalen;
		uint8_t *reladata = (uint8_t *)data;

		relalen = len - r;

		reladata = reladata + r;
		relaoffset = offset + r;

		round = CACHEBLOCKSZ - relaoffset % CACHEBLOCKSZ;
		if (relalen > round)
			relalen = round;

		for (leaf = cache->list; leaf && leaf->next; leaf = leaf->next) {
			if ((leaf->next->blockid * CACHEBLOCKSZ) > relaoffset)
				break;
		}

		relablock = relaoffset / ((float)CACHEBLOCKSZ) - leaf->blockid;

		if ((relablock >= 0) && (relablock < 1))
			memcpy(data + r, leaf->data + (relaoffset % CACHEBLOCKSZ), relalen);

		r = r + relalen;
	}

	return 0;
}

uint32_t filecache_push(pFileCache_t cache, uint32_t offset, uint32_t len, const uint8_t *data)
{
	uint16_t i;
	float blocks;
	uint32_t r = 0;
	uint8_t updateroot = 0x1;

	blocks = (offset % CACHEBLOCKSZ + len) / (float)CACHEBLOCKSZ;

	if (blocks == 0.0)
		return 0;

	if ((blocks - (int)blocks) > 0.0)
		blocks = blocks + 1.0;

	for (i = 0; i < (uint16_t)blocks; i++) {
		uint16_t round;
		uint32_t localr;
		linkedlist_t *leaf;
		uint32_t relaoffset, relalen;
		uint8_t *reladata = (uint8_t *)data;

		relalen = len - r;

		reladata = reladata + r;
		relaoffset = offset + r;

		round = CACHEBLOCKSZ - relaoffset % CACHEBLOCKSZ;
		if (relalen > round)
			relalen = round;

		for (leaf = cache->list; leaf && leaf->next; leaf = leaf->next) {
			if ((leaf->next->blockid * CACHEBLOCKSZ) > relaoffset)
				break;
			updateroot = 0x0;
		}

		localr = linkedlist_store(&leaf, relaoffset,
					  (relalen > CACHEBLOCKSZ) ? CACHEBLOCKSZ : relalen,
					  reladata);
		if (localr == SQLITE_NOMEM)
			return SQLITE_NOMEM;

		r = r + localr;

		if (updateroot & 0x1)
			cache->list = leaf;
	}

	if (offset + len > cache->size)
		cache->size = offset + len;

	return r;
}

void filecache_free(pFileCache_t cache)
{
	pLinkedList_t ll = cache->list, next;

	while (ll != NULL) {
		next = ll->next;
		sqlite3_free(ll);
		ll = next;
	}
}

int cskmem_Close(sqlite3_file *id)
{
	csk_file *file = (csk_file *)id;
	// printf("cskmem_Close: file->fd %p\n", file->fd);
	// lsfs_close(file->fd);
	filecache_free(file->cache);
	sqlite3_free(file->cache);

	dbg_printf("cskmem_Close: %s OK\n", file->name);
	return SQLITE_OK;
}

int cskmem_Read(sqlite3_file *id, void *buffer, int amount, sqlite3_int64 offset)
{
	int32_t ofst;
	csk_file *file = (csk_file *)id;
	ofst = (int32_t)(offset & 0x7FFFFFFF);

	filecache_pull(file->cache, ofst, amount, (uint8_t *)buffer);

	dbg_printf("cskmem_Read: %s [%d] [%d] OK\n", file->name, ofst, amount);
	return SQLITE_OK;
}

int cskmem_Write(sqlite3_file *id, const void *buffer, int amount, sqlite3_int64 offset)
{
	int32_t ofst;
	csk_file *file = (csk_file *)id;

	ofst = (int32_t)(offset & 0x7FFFFFFF);

	filecache_push(file->cache, ofst, amount, (const uint8_t *)buffer);

	dbg_printf("cskmem_Write: %s [%d] [%d] OK\n", file->name, ofst, amount);
	return SQLITE_OK;
}

int cskmem_Sync(sqlite3_file *id, int flags)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	dbg_printf("cskmem_Sync: %s OK\n", file->name);
	return SQLITE_OK;
}

int cskmem_FileSize(sqlite3_file *id, sqlite3_int64 *size)
{
	csk_file *file = (csk_file *)id;

	*size = 0LL | file->cache->size;
	dbg_printf("cskmem_FileSize: %s [%d] OK\n", file->name, file->cache->size);
	return SQLITE_OK;
}

int csk_Open(sqlite3_vfs *vfs, const char *path, sqlite3_file *file, int flags, int *outflags)
{
	int rc;

	csk_file *p = (csk_file *)file;

	lsfs_mode_t mode = LSFS_O_READ;

	if (path == NULL)
		return SQLITE_IOERR;
	dbg_printf("csk_Open: 0o %s %0x\n", path, mode);
	if (flags & SQLITE_OPEN_READONLY)
		mode |= LSFS_O_READ;
	if (flags & SQLITE_OPEN_READWRITE || flags & SQLITE_OPEN_MAIN_JOURNAL) {
		int result;
		if (SQLITE_OK != csk_Access(vfs, path, flags, &result))
			return SQLITE_CANTOPEN;
		mode |= LSFS_O_RDWR;
	}

	dbg_printf("csk_Open: 1o %s %0x\n", path, mode);
	memset(p, 0, sizeof(csk_file));

	strncpy(p->name, path, csk_DEFAULT_MAXNAMESIZE);
	p->name[csk_DEFAULT_MAXNAMESIZE - 1] = '\0';

	if (flags & SQLITE_OPEN_MAIN_JOURNAL) {
		p->fd = 0;
		p->cache = (filecache_t *)sqlite3_malloc(sizeof(filecache_t));
		if (!p->cache)
			return SQLITE_NOMEM;
		memset(p->cache, 0, sizeof(filecache_t));

		p->base.pMethods = &cskMemMethods;
		dbg_printf("csk_Open: 2o %s MEM OK\n", p->name);
		return SQLITE_OK;
	} else {
		p->fd = (struct lsfs_file_t *)__sq_malloc(sizeof(struct lsfs_file_t));
		if (!p->fd)
			return SQLITE_NOMEM;
		lsfs_file_t_init(p->fd);
	}

	rc = lsfs_open(p->fd, path, mode);
	if (rc < 0) {
		dbg_printf("FAIL: open %s: %d\n", path, rc);
		return SQLITE_CANTOPEN;
	}
	p->base.pMethods = &cskIoMethods;
	dbg_printf("csk_Open: 2o %s OK\n", p->name);
	return SQLITE_OK;
}

int csk_Close(sqlite3_file *id)
{
	csk_file *file = (csk_file *)id;

	// int rc = fclose(file->fd);
	int rc = lsfs_close(file->fd);
	__sq_free(file->fd);
	dbg_printf("csk_Close: %s %d\n", file->name, rc);
	return rc ? SQLITE_IOERR_CLOSE : SQLITE_OK;
}

int csk_Read(sqlite3_file *id, void *buffer, int amount, sqlite3_int64 offset)
{
	size_t nRead;
	int32_t ofst, iofst;
	csk_file *file = (csk_file *)id;

	iofst = (int32_t)(offset & 0x7FFFFFFF);

	dbg_printf("csk_Read: 1r %s %d %lld[%d] \n", file->name, amount, offset, iofst);
	ofst = lsfs_seek(file->fd, iofst, SEEK_SET);
	if (ofst != 0) {
		dbg_printf("csk_Read: 2r %d != %d FAIL\n", ofst, iofst);
		return SQLITE_IOERR_SHORT_READ /* SQLITE_IOERR_SEEK */;
	}

	nRead = lsfs_read(file->fd, buffer, amount);
	if (nRead == amount) {
		dbg_printf("csk_Read: 3r %s %u %d OK\n", file->name, nRead, amount);
		return SQLITE_OK;
	} else if (nRead >= 0) {
		dbg_printf("csk_Read: 3r %s %u %d FAIL\n", file->name, nRead, amount);
		return SQLITE_IOERR_SHORT_READ;
	}

	dbg_printf("csk_Read: 4r %s FAIL\n", file->name);
	return SQLITE_IOERR_READ;
}

int csk_Write(sqlite3_file *id, const void *buffer, int amount, sqlite3_int64 offset)
{
	size_t nWrite;
	int32_t ofst, iofst;
	csk_file *file = (csk_file *)id;

	iofst = (int32_t)(offset & 0x7FFFFFFF);

	dbg_printf("csk_Write: 1w %s %d %lld[%d] \n", file->name, amount, offset, iofst);
	ofst = lsfs_seek(file->fd, iofst, SEEK_SET);
	if (ofst != 0) {
		return SQLITE_IOERR_SEEK;
	}

	dbg_printf("csk_Write: 1.1w %s %d %lld[%d] \n", file->name, amount, offset, iofst);

	nWrite = lsfs_write(file->fd, buffer, amount);
	if (nWrite != amount) {
		dbg_printf("csk_Write: 2w %s %u %d\n", file->name, nWrite, amount);
		return SQLITE_IOERR_WRITE;
	}

	dbg_printf("csk_Write: 3w %s OK\n", file->name);
	return SQLITE_OK;
}

int csk_Truncate(sqlite3_file *id, sqlite3_int64 bytes)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	int fno = lsfs_truncate(file->fd, bytes);
	if (fno != 0)
		return SQLITE_IOERR_TRUNCATE;
	//if (ftruncate(fno, 0))
	//	return SQLITE_IOERR_TRUNCATE;


	dbg_printf("csk_Truncate:\n");
	return SQLITE_OK;
}

int csk_Delete(sqlite3_vfs *vfs, const char *path, int syncDir)
{
	// dbg_printf("csk_Delete: start delete\n");
	struct lsfs_dirent dirent;
	int32_t rc = SQLITE_IOERR_DELETE;
	if (lsfs_stat(path, &dirent) >= 0) {
		rc = lsfs_unlink(path);
	}
	if (rc)
		return SQLITE_IOERR_DELETE;

	dbg_printf("csk_Delete: %s OK\n", path);
	return SQLITE_OK;
}

int csk_FileSize(sqlite3_file *id, sqlite3_int64 *size)
{
	csk_file *file = (csk_file *)id;
	dbg_printf("csk_FileSize: %s: ", file->name);
	struct lsfs_dirent st;
	// int fno = fileno(file->fd);
	// if (fno == -1)
	// 	return SQLITE_IOERR_FSTAT;
	if (lsfs_stat(file->name, &st))
		return SQLITE_IOERR_FSTAT;
	*size = st.size;
	dbg_printf(" %d[%lld]\n", st.size, *size);
	return SQLITE_OK;
}

int csk_Sync(sqlite3_file *id, int flags)
{
	csk_file *file = (csk_file *)id;

	// int rc = fflush( file->fd );
	lsfs_sync(file->fd);
	dbg_printf("csk_Sync( %s: ) \n", file->name);

	return SQLITE_OK;
}

int csk_Access(sqlite3_vfs *vfs, const char *path, int flags, int *result)
{
	struct lsfs_dirent st;
	memset(&st, 0, sizeof(struct lsfs_dirent));
	int rc = lsfs_stat(path, &st);
	// *result = ( rc != -1 );
	*result = (rc >= 0);

	dbg_printf("csk_Access: %s %d %d %d\n", path, *result, rc, st.size);
	return SQLITE_OK;
}

int csk_FullPathname(sqlite3_vfs *vfs, const char *path, int len, char *fullpath)
{
	//structure stat does not have name.
	//struct stat st;
	//int32_t rc = stat( path, &st );
	//if ( rc == 0 ){
	//	strncpy( fullpath, st.name, len );
	//} else {
	//	strncpy( fullpath, path, len );
	//}

	// As now just copy the path
	strncpy(fullpath, path, len);
	fullpath[len - 1] = '\0';

	dbg_printf("csk_FullPathname: %s\n", fullpath);
	return SQLITE_OK;
}

int csk_Lock(sqlite3_file *id, int lock_type)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	dbg_printf("csk_Lock:Not locked\n");
	return SQLITE_OK;
}

int csk_Unlock(sqlite3_file *id, int lock_type)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	dbg_printf("csk_Unlock:\n");
	return SQLITE_OK;
}

int csk_CheckReservedLock(sqlite3_file *id, int *result)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	*result = 0;

	dbg_printf("csk_CheckReservedLock:\n");
	return SQLITE_OK;
}

int csk_FileControl(sqlite3_file *id, int op, void *arg)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	dbg_printf("csk_FileControl:\n");
	return SQLITE_OK;
}

int csk_SectorSize(sqlite3_file *id)
{
	// csk_file *file = (csk_file*) id;

	// dbg_printf("csk_SectorSize:\n");
	// return SPI_FLASH_SEC_SIZE;
	return 1024; // todo
}

int csk_DeviceCharacteristics(sqlite3_file *id)
{
	csk_file *file = (csk_file *)id;
	UNUSED(file);
	dbg_printf("csk_DeviceCharacteristics:\n");
	return 0;
}

void *csk_DlOpen(sqlite3_vfs *vfs, const char *path)
{
	dbg_printf("csk_DlOpen:\n");
	return NULL;
}

void csk_DlError(sqlite3_vfs *vfs, int len, char *errmsg)
{
	dbg_printf("csk_DlError:\n");
	return;
}

void (*csk_DlSym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void)
{
	dbg_printf("csk_DlSym:\n");
	return NULL;
}

void csk_DlClose(sqlite3_vfs *vfs, void *handle)
{
	dbg_printf("csk_DlClose:\n");
	return;
}

int csk_Randomness(sqlite3_vfs *vfs, int len, char *buffer)
{
	long rdm;
	int sz = 1 + (len / sizeof(long));
	char a_rdm[sz * sizeof(long)];
	while (sz--) {
		rdm = rand();
		memcpy(a_rdm + sz * sizeof(long), &rdm, sizeof(long));
	}
	memcpy(buffer, a_rdm, len);
	dbg_printf("csk_Randomness\n");
	return SQLITE_OK;
}

int csk_Sleep(sqlite3_vfs *vfs, int microseconds)
{
	// ets_delay_us(microseconds);
	dbg_printf("csk_Sleep:\n");
	return SQLITE_OK;
}

int csk_CurrentTime(sqlite3_vfs *vfs, double *result)
{
	// todo
	// time_t t = time(NULL);
	// *result = t / 86400.0 + 2440587.5;
	// // This is stubbed out until we have a working RTCTIME solution;
	// // as it stood, this would always have returned the UNIX epoch.
	// //*result = 2440587.5;
	// dbg_printf("csk_CurrentTime: %g\n", *result);
	return SQLITE_OK;
}

static void shox96_0_2c(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	int nIn, nOut;
	long int nOut2;
	const unsigned char *inBuf;
	unsigned char *outBuf;
	unsigned char vInt[9];
	int vIntLen;

	//   assert( argc==1 );
	if (argc != 1) {
		printf("shox96_0_2c: error\n");
		return;
	}
	nIn = sqlite3_value_bytes(argv[0]);
	inBuf = (unsigned char *)sqlite3_value_blob(argv[0]);
	nOut = 13 + nIn + (nIn + 999) / 1000;
	vIntLen = encode_unsigned_varint(vInt, (uint64_t)nIn);

	outBuf = (unsigned char *)__sq_malloc(nOut + vIntLen);
	memcpy(outBuf, vInt, vIntLen);
	nOut2 = shox96_0_2_compress((const char *)inBuf, nIn, (char *)&outBuf[vIntLen], NULL);
	sqlite3_result_blob(context, outBuf, nOut2 + vIntLen, __sq_free);
}

static void shox96_0_2d(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	unsigned int nIn, nOut;
	const unsigned char *inBuf;
	unsigned char *outBuf;
	long int nOut2;
	uint64_t inBufLen64;
	int vIntLen;

	//   assert( argc==1 );
	if (argc != 1) {
		printf("shox96_0_2d: error\n");
		return;
	}

	if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
		return;

	nIn = sqlite3_value_bytes(argv[0]);
	if (nIn < 2) {
		return;
	}
	inBuf = (unsigned char *)sqlite3_value_blob(argv[0]);
	inBufLen64 = decode_unsigned_varint(inBuf, &vIntLen);
	nOut = (unsigned int)inBufLen64;
	outBuf = (unsigned char *)__sq_malloc(nOut);
	//nOut2 = (long int)nOut;
	nOut2 = shox96_0_2_decompress((const char *)(inBuf + vIntLen), nIn - vIntLen,
				      (char *)outBuf, NULL);
	//if( rc!=Z_OK ){
	//  free(outBuf);
	//}else{
	sqlite3_result_blob(context, outBuf, nOut2, __sq_free);
	//}
}

int registerShox96_0_2(sqlite3 *db, const char **pzErrMsg,
		       const struct sqlite3_api_routines *pThunk)
{
	sqlite3_create_function(db, "shox96_0_2c", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
				shox96_0_2c, 0, 0);
	sqlite3_create_function(db, "shox96_0_2d", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
				shox96_0_2d, 0, 0);
	return SQLITE_OK;
}

int sqlite3_os_init(void)
{
	sqlite3_vfs_register(&cskVfs, 1);
	sqlite3_auto_extension((void (*)())registerShox96_0_2);
	return SQLITE_OK;
}

int sqlite3_os_end(void)
{
	return SQLITE_OK;
}
