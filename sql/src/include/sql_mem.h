/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://monetdb.cwi.nl/Legal/MonetDBLicense-1.1.html
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-2005 CWI.
 * All Rights Reserved.
 */

#ifndef _MEM_H_
#define _MEM_H_

/*#ifdef _MSC_VER*/
#include <sql_config.h>
/*#endif*/

#include <gdk.h>
#include <stdio.h>
#include <assert.h>

#define SQL_OK 	GDK_SUCCEED
#define SQL_ERR GDK_FAIL

#ifdef _MSC_VER
#ifndef LIBSQLSERVER
#define sql_export extern __declspec(dllimport)
#else
#define sql_export extern __declspec(dllexport)
#endif
#ifndef LIBSQLCOMMON
#define sqlcommon_export extern __declspec(dllimport)
#else
#define sqlcommon_export extern __declspec(dllexport)
#endif
#ifndef LIBBATSTORE
#define sqlbat_export extern __declspec(dllimport)
#else
#define sqlbat_export extern __declspec(dllexport)
#endif
#else
#define sql_export extern
#define sqlcommon_export extern
#define sqlbat_export extern
#endif

#define NEW( type ) (type*)GDKmalloc(sizeof(type) )
#define NEW_ARRAY( type, size ) (type*)GDKmalloc((size)*sizeof(type))
#define RENEW_ARRAY( type,ptr,size) (type*)GDKrealloc((void*)ptr,(size)*sizeof(type))

#define NEWADT( size ) (adt*)GDKmalloc(size)
#define _DELETE( ptr )	{ GDKfree(ptr); ptr = NULL; }
#define _strdup( ptr )	GDKstrdup((char*)ptr)

typedef struct sql_ref {
	int refcnt;
} sql_ref;

extern sql_ref *sql_ref_init(sql_ref *r);
extern int sql_ref_inc(sql_ref *r);
extern int sql_ref_dec(sql_ref *r);

typedef struct sql_allocator {
	size_t size;
	size_t nr;
	char **blks;
	size_t used; 	/* memory used in last block */
} sql_allocator;

extern sql_allocator *sa_create();
extern sql_allocator *sa_reset( sql_allocator *sa );
extern char *sa_alloc( sql_allocator *sa,  size_t sz );
extern void sa_destroy( sql_allocator *sa );
extern char *sa_strndup( sql_allocator *sa, const char *s, int l);
extern char *sa_strdup( sql_allocator *sa, const char *s);

#define SA_NEW( sa, type ) ((type*)sa_alloc( sa, sizeof(type)) )

#endif /*_MEM_H_*/
