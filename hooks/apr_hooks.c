/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "apr_pools.h"
#include "apr_tables.h"
#include "apr.h"
#include "apr_hooks.h"
#include "apr_hash.h"
#include "apr_optional_hooks.h"
#include "apr_optional.h"
#define APR_WANT_MEMFUNC
#define APR_WANT_STRFUNC
#include "apr_want.h"

#if 0
#define apr_palloc(pool,size)	malloc(size)
#endif

APU_DECLARE_DATA apr_pool_t *apr_global_hook_pool = NULL;
APU_DECLARE_DATA int apr_debug_module_hooks = 0;
APU_DECLARE_DATA const char *apr_current_hooking_module = NULL;

/* NB: This must echo the LINK_##name structure */
typedef struct
{
    void (*dummy)(void *);
    const char *szName;
    const char * const *aszPredecessors;
    const char * const *aszSuccessors;
    int nOrder;
} TSortData;

typedef struct tsort_
{
    void *pData;
    int nPredecessors;
    struct tsort_ **ppPredecessors;
    struct tsort_ *pNext;
} TSort;

static int crude_order(const void *a_,const void *b_)
{
    const TSortData *a=a_;
    const TSortData *b=b_;

    return a->nOrder-b->nOrder;
}

static TSort *prepare(apr_pool_t *p,TSortData *pItems,int nItems)
{
    TSort *pData=apr_palloc(p,nItems*sizeof *pData);
    int n;
    
    qsort(pItems,nItems,sizeof *pItems,crude_order);
    for(n=0 ; n < nItems ; ++n) {
	pData[n].nPredecessors=0;
	pData[n].ppPredecessors=apr_pcalloc(p,nItems*sizeof *pData[n].ppPredecessors);
	pData[n].pNext=NULL;
	pData[n].pData=&pItems[n];
    }

    for(n=0 ; n < nItems ; ++n) {
	int i,k;

	for(i=0 ; pItems[n].aszPredecessors && pItems[n].aszPredecessors[i] ; ++i)
	    for(k=0 ; k < nItems ; ++k)
		if(!strcmp(pItems[k].szName,pItems[n].aszPredecessors[i])) {
		    int l;

		    for(l=0 ; l < pData[n].nPredecessors ; ++l)
			if(pData[n].ppPredecessors[l] == &pData[k])
			    goto got_it;
		    pData[n].ppPredecessors[pData[n].nPredecessors]=&pData[k];
		    ++pData[n].nPredecessors;
		got_it:
		    break;
		}
	for(i=0 ; pItems[n].aszSuccessors && pItems[n].aszSuccessors[i] ; ++i)
	    for(k=0 ; k < nItems ; ++k)
		if(!strcmp(pItems[k].szName,pItems[n].aszSuccessors[i])) {
		    int l;

		    for(l=0 ; l < pData[k].nPredecessors ; ++l)
			if(pData[k].ppPredecessors[l] == &pData[n])
			    goto got_it2;
		    pData[k].ppPredecessors[pData[k].nPredecessors]=&pData[n];
		    ++pData[k].nPredecessors;
		got_it2:
		    break;
		}
    }

    return pData;
}

static TSort *tsort(TSort *pData,int nItems)
{
    int nTotal;
    TSort *pHead=NULL;
    TSort *pTail=NULL;

    for(nTotal=0 ; nTotal < nItems ; ++nTotal) {
	int n,i,k;

	for(n=0 ; ; ++n) {
	    if(n == nItems)
		assert(0);      /* we have a loop... */
	    if(!pData[n].pNext && !pData[n].nPredecessors)
		break;
	}
	if(pTail)
	    pTail->pNext=&pData[n];
	else
	    pHead=&pData[n];
	pTail=&pData[n];
	pTail->pNext=pTail;     /* fudge it so it looks linked */
	for(i=0 ; i < nItems ; ++i)
	    for(k=0 ; pData[i].ppPredecessors[k] ; ++k)
		if(pData[i].ppPredecessors[k] == &pData[n]) {
		    --pData[i].nPredecessors;
		    break;
		}
    }
    pTail->pNext=NULL;  /* unfudge the tail */
    return pHead;
}

static apr_array_header_t *sort_hook(apr_array_header_t *pHooks,
				     const char *szName)
{
    apr_pool_t *p;
    TSort *pSort;
    apr_array_header_t *pNew;
    int n;

    apr_pool_create(&p, apr_global_hook_pool);
    pSort=prepare(p,(TSortData *)pHooks->elts,pHooks->nelts);
    pSort=tsort(pSort,pHooks->nelts);
    pNew=apr_array_make(apr_global_hook_pool,pHooks->nelts,sizeof(TSortData));
    if(apr_debug_module_hooks)
	printf("Sorting %s:",szName);
    for(n=0 ; pSort ; pSort=pSort->pNext,++n) {
	TSortData *pHook;
	assert(n < pHooks->nelts);
	pHook=apr_array_push(pNew);
	memcpy(pHook,pSort->pData,sizeof *pHook);
	if(apr_debug_module_hooks)
	    printf(" %s",pHook->szName);
    }
    if(apr_debug_module_hooks)
	fputc('\n',stdout);
    return pNew;
}

static apr_array_header_t *s_aHooksToSort;
typedef struct
{
    const char *szHookName;
    apr_array_header_t **paHooks;
} HookSortEntry;

APU_DECLARE(void) apr_hook_sort_register(const char *szHookName,
					apr_array_header_t **paHooks)
{
    HookSortEntry *pEntry;

    if(!s_aHooksToSort)
	s_aHooksToSort=apr_array_make(apr_global_hook_pool,1,sizeof(HookSortEntry));
    pEntry=apr_array_push(s_aHooksToSort);
    pEntry->szHookName=szHookName;
    pEntry->paHooks=paHooks;
}

APU_DECLARE(void) apr_sort_hooks()
{
    int n;

    for(n=0 ; n < s_aHooksToSort->nelts ; ++n) {
	HookSortEntry *pEntry=&((HookSortEntry *)s_aHooksToSort->elts)[n];
	*pEntry->paHooks=sort_hook(*pEntry->paHooks,pEntry->szHookName);
    }
}
    
static apr_hash_t *s_phOptionalHooks;
static apr_hash_t *s_phOptionalFunctions;

APU_DECLARE(void) apr_hook_deregister_all(void)
{
    int n;    

    for(n=0 ; n < s_aHooksToSort->nelts ; ++n) {
        HookSortEntry *pEntry=&((HookSortEntry *)s_aHooksToSort->elts)[n];
        *pEntry->paHooks=NULL;
    }
    s_aHooksToSort=NULL;
    s_phOptionalHooks=NULL;
    s_phOptionalFunctions=NULL;
}

APU_DECLARE(void) apr_show_hook(const char *szName,const char * const *aszPre,
			       const char * const *aszSucc)
{
    int nFirst;

    printf("  Hooked %s",szName);
    if(aszPre) {
	fputs(" pre(",stdout);
	nFirst=1;
	while(*aszPre) {
	    if(!nFirst)
		fputc(',',stdout);
	    nFirst=0;
	    fputs(*aszPre,stdout);
	    ++aszPre;
	}
	fputc(')',stdout);
    }
    if(aszSucc) {
	fputs(" succ(",stdout);
	nFirst=1;
	while(*aszSucc) {
	    if(!nFirst)
		fputc(',',stdout);
	    nFirst=0;
	    fputs(*aszSucc,stdout);
	    ++aszSucc;
	}
	fputc(')',stdout);
    }
    fputc('\n',stdout);
}

/* Optional hook support */

APR_DECLARE_EXTERNAL_HOOK(apr,APU,void,_optional,(void))

APU_DECLARE(apr_array_header_t *) apr_optional_hook_get(const char *szName)
{
    apr_array_header_t **ppArray;

    if(!s_phOptionalHooks)
	return NULL;
    ppArray=apr_hash_get(s_phOptionalHooks,szName,strlen(szName));
    if(!ppArray)
	return NULL;
    return *ppArray;
}

APU_DECLARE(void) apr_optional_hook_add(const char *szName,void (*pfn)(void),
					const char * const *aszPre,
					const char * const *aszSucc,int nOrder)
{
    apr_array_header_t *pArray=apr_optional_hook_get(szName);
    apr_LINK__optional_t *pHook;

    if(!pArray) {
	apr_array_header_t **ppArray;

	pArray=apr_array_make(apr_global_hook_pool,1,
			      sizeof(apr_LINK__optional_t));
	if(!s_phOptionalHooks)
	    s_phOptionalHooks=apr_hash_make(apr_global_hook_pool);
	ppArray=apr_palloc(apr_global_hook_pool,sizeof *ppArray);
	*ppArray=pArray;
	apr_hash_set(s_phOptionalHooks,szName,strlen(szName),ppArray);
	apr_hook_sort_register(szName,ppArray);
    }
    pHook=apr_array_push(pArray);
    pHook->pFunc=pfn;
    pHook->aszPredecessors=aszPre;
    pHook->aszSuccessors=aszSucc;
    pHook->nOrder=nOrder;
    pHook->szName=apr_current_hooking_module;
    if(apr_debug_module_hooks)
	apr_show_hook(szName,aszPre,aszSucc);
}

/* optional function support */

APU_DECLARE(apr_opt_fn_t *) apr_retrieve_optional_fn(const char *szName)
{
    if(!s_phOptionalFunctions)
	return NULL;
    return (void(*)(void))apr_hash_get(s_phOptionalFunctions,szName,strlen(szName));
}

APU_DECLARE_NONSTD(void) apr_register_optional_fn(const char *szName,
                                                  apr_opt_fn_t *pfn)
{
    if(!s_phOptionalFunctions)
	s_phOptionalFunctions=apr_hash_make(apr_global_hook_pool);
    apr_hash_set(s_phOptionalFunctions,szName,strlen(szName),(void *)pfn);
}

#if 0
void main()
{
    const char *aszAPre[]={"b","c",NULL};
    const char *aszBPost[]={"a",NULL};
    const char *aszCPost[]={"b",NULL};
    TSortData t1[]=
    {
	{ "a",aszAPre,NULL },
	{ "b",NULL,aszBPost },
	{ "c",NULL,aszCPost }
    };
    TSort *pResult;

    pResult=prepare(t1,3);
    pResult=tsort(pResult,3);

    for( ; pResult ; pResult=pResult->pNext)
	printf("%s\n",pResult->pData->szName);
}
#endif
