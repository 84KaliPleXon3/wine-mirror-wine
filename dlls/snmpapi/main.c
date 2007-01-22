/*
 * Implementation of SNMPAPI.DLL
 *
 * Copyright 2002 Patrik Stridvall
 * Copyright 2007 Hans Leidekker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "snmp.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(snmpapi);

static INT asn_any_copy(AsnAny *dst, AsnAny *src)
{
    memset(dst, 0, sizeof(AsnAny));
    switch (src->asnType)
    {
    case ASN_INTEGER32:  dst->asnValue.number = src->asnValue.number; break;
    case ASN_UNSIGNED32: dst->asnValue.unsigned32 = src->asnValue.unsigned32; break;
    case ASN_COUNTER64:  dst->asnValue.counter64 = src->asnValue.counter64; break;
    case ASN_COUNTER32:  dst->asnValue.counter = src->asnValue.counter; break;
    case ASN_GAUGE32:    dst->asnValue.gauge = src->asnValue.gauge; break;
    case ASN_TIMETICKS:  dst->asnValue.ticks = src->asnValue.ticks; break;

    case ASN_OCTETSTRING:
    case ASN_BITS:
    case ASN_SEQUENCE:
    case ASN_IPADDRESS:
    case ASN_OPAQUE:
    {
        BYTE *stream;
        UINT length = src->asnValue.string.length;

        if (!(stream = HeapAlloc(GetProcessHeap(), 0, length))) return SNMPAPI_ERROR;

        dst->asnValue.string.stream = stream;
        dst->asnValue.string.length = length;
        dst->asnValue.string.dynamic = TRUE;
        break;
    }
    case ASN_OBJECTIDENTIFIER:
    {
        UINT *ids, i, size = src->asnValue.object.idLength * sizeof(UINT);

        if (!(ids = HeapAlloc(GetProcessHeap(), 0, size))) return SNMPAPI_ERROR;

        dst->asnValue.object.ids = ids;
        dst->asnValue.object.idLength = src->asnValue.object.idLength;

        for (i = 0; i < dst->asnValue.object.idLength; i++)
            dst->asnValue.object.ids[i] = src->asnValue.object.ids[i];
        break;
    }
    default:
    {
        WARN("unknown ASN type: %d\n", src->asnType);
        return SNMPAPI_ERROR;
    }
    }
    dst->asnType = src->asnType;
    return SNMPAPI_NOERROR;
}

static void asn_any_free(AsnAny *any)
{
    switch (any->asnType)
    {
    case ASN_OCTETSTRING:
    case ASN_BITS:
    case ASN_SEQUENCE:
    case ASN_IPADDRESS:
    case ASN_OPAQUE:
    {
        if (any->asnValue.string.dynamic)
        {
            HeapFree(GetProcessHeap(), 0, any->asnValue.string.stream);
            any->asnValue.string.stream = NULL;
        }
        break;
    }
    case ASN_OBJECTIDENTIFIER:
    {
        HeapFree(GetProcessHeap(), 0, any->asnValue.object.ids);
        any->asnValue.object.ids = NULL;
        break;
    }
    default: break;
    }
    any->asnType = ASN_NULL;
}

/***********************************************************************
 *		DllMain for SNMPAPI
 */
BOOL WINAPI DllMain(
	HINSTANCE hInstDLL,
	DWORD fdwReason,
	LPVOID lpvReserved)
{
    TRACE("(%p,%d,%p)\n", hInstDLL, fdwReason, lpvReserved);

    switch(fdwReason) {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDLL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

/***********************************************************************
 *      SnmpUtilMemAlloc        (SNMPAPI.@)
 */
LPVOID WINAPI SnmpUtilMemAlloc(UINT nbytes)
{
    TRACE("(%d)\n", nbytes);
    return HeapAlloc(GetProcessHeap(), 0, nbytes);
}

/***********************************************************************
 *      SnmpUtilMemReAlloc      (SNMPAPI.@)
 */
LPVOID WINAPI SnmpUtilMemReAlloc(LPVOID mem, UINT nbytes)
{
    TRACE("(%p, %d)\n", mem, nbytes);
    return HeapReAlloc(GetProcessHeap(), 0, mem, nbytes);
}

/***********************************************************************
 *      SnmpUtilMemFree         (SNMPAPI.@)
 */
void WINAPI SnmpUtilMemFree(LPVOID mem)
{
    TRACE("(%p)\n", mem);
    HeapFree(GetProcessHeap(), 0, mem);
}

/***********************************************************************
 *      SnmpUtilAsnAnyCpy       (SNMPAPI.@)
 */
INT WINAPI SnmpUtilAsnAnyCpy(AsnAny *dst, AsnAny *src)
{
    TRACE("(%p, %p)\n", dst, src);
    return asn_any_copy(dst, src);
}

/***********************************************************************
 *      SnmpUtilAsnAnyFree      (SNMPAPI.@)
 */
void WINAPI SnmpUtilAsnAnyFree(AsnAny *any)
{
    TRACE("(%p)\n", any);
    asn_any_free(any);
}

/***********************************************************************
 *      SnmpUtilOctetsCpy       (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOctetsCpy(AsnOctetString *dst, AsnOctetString *src)
{
    TRACE("(%p, %p)\n", dst, src);

    if (!dst) return SNMPAPI_ERROR;
    if (!src)
    {
        dst->dynamic = FALSE;
        dst->length = 0;
        dst->stream = NULL;
        return SNMPAPI_NOERROR;
    }
    if ((dst->stream = HeapAlloc(GetProcessHeap(), 0, src->length)))
    {
        unsigned int i;

        dst->dynamic = TRUE;
        dst->length = src->length;
        for (i = 0; i < dst->length; i++) dst->stream[i] = src->stream[i];
        return SNMPAPI_NOERROR;
    }
    return SNMPAPI_ERROR;
}

/***********************************************************************
 *      SnmpUtilOctetsFree      (SNMPAPI.@)
 */
void WINAPI SnmpUtilOctetsFree(AsnOctetString *octets)
{
    TRACE("(%p)\n", octets);

    if (octets)
    {
        octets->length = 0;
        if (octets->dynamic) HeapFree(GetProcessHeap(), 0, octets->stream);
        octets->stream = NULL;
        octets->dynamic = FALSE;
    }
}

/***********************************************************************
 *      SnmpUtilOctetsNCmp      (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOctetsNCmp(AsnOctetString *octets1, AsnOctetString *octets2, UINT count)
{
    INT ret;
    unsigned int i;

    TRACE("(%p, %p, %d)\n", octets1, octets2, count);

    if (!octets1 || !octets2) return 0;

    for (i = 0; i < count; i++)
        if ((ret = octets1->stream[i] - octets2->stream[i])) return ret;

    return 0;
}

/***********************************************************************
 *      SnmpUtilOctetsCmp       (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOctetsCmp(AsnOctetString *octets1, AsnOctetString *octets2)
{
    TRACE("(%p, %p)\n", octets1, octets2);

    if (octets1->length < octets2->length) return -1;
    if (octets1->length > octets2->length) return 1;

    return SnmpUtilOctetsNCmp(octets1, octets2, octets1->length);
}

/***********************************************************************
 *      SnmpUtilOidAppend       (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOidAppend(AsnObjectIdentifier *dst, AsnObjectIdentifier *src)
{
    UINT *ids, i, size;

    TRACE("(%p, %p)\n", dst, src);

    if (!dst) return SNMPAPI_ERROR;
    if (!src) return SNMPAPI_NOERROR;

    size = (src->idLength + dst->idLength) * sizeof(UINT);
    if (!(ids = HeapReAlloc(GetProcessHeap(), 0, dst->ids, size)))
    {
        if (!(ids = HeapAlloc(GetProcessHeap(), 0, size)))
        {
            SetLastError(SNMP_MEM_ALLOC_ERROR);
            return SNMPAPI_ERROR;
        }
        else memcpy(ids, dst->ids, dst->idLength * sizeof(UINT));
    }

    for (i = 0; i < src->idLength; i++) ids[i + dst->idLength] = src->ids[i];
    dst->idLength = dst->idLength + src->idLength;
    dst->ids = ids;

    return SNMPAPI_NOERROR;
}

/***********************************************************************
 *      SnmpUtilOidCpy          (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOidCpy(AsnObjectIdentifier *dst, AsnObjectIdentifier *src)
{
    TRACE("(%p, %p)\n", dst, src);

    if (!dst) return SNMPAPI_ERROR;
    if (!src)
    {
        dst->idLength = 0;
        dst->ids = NULL;
        return SNMPAPI_NOERROR;
    }
    if ((dst->ids = HeapAlloc(GetProcessHeap(), 0, src->idLength * sizeof(UINT))))
    {
        unsigned int i;

        dst->idLength = src->idLength;
        for (i = 0; i < dst->idLength; i++) dst->ids[i] = src->ids[i];
        return SNMPAPI_NOERROR;
    }
    return SNMPAPI_ERROR;
}

/***********************************************************************
 *      SnmpUtilOidFree         (SNMPAPI.@)
 */
void WINAPI SnmpUtilOidFree(AsnObjectIdentifier *oid)
{
    TRACE("(%p)\n", oid);

    if (!oid) return;

    oid->idLength = 0;
    HeapFree(GetProcessHeap(), 0, oid->ids);
    oid->ids = NULL;
}

/***********************************************************************
 *      SnmpUtilOidNCmp         (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOidNCmp(AsnObjectIdentifier *oid1, AsnObjectIdentifier *oid2, UINT count)
{
    unsigned int i;

    TRACE("(%p, %p, %d)\n", oid1, oid2, count);

    if (!oid1 || !oid2) return 0;

    for (i = 0; i < count; i++)
    {
        if (oid1->ids[i] > oid2->ids[i]) return 1;
        if (oid1->ids[i] < oid2->ids[i]) return -1;
    }
    return 0;
}

/***********************************************************************
 *      SnmpUtilOidCmp          (SNMPAPI.@)
 */
INT WINAPI SnmpUtilOidCmp(AsnObjectIdentifier *oid1, AsnObjectIdentifier *oid2)
{
    TRACE("(%p, %p)\n", oid1, oid2);

    if (oid1->idLength < oid2->idLength) return -1;
    if (oid1->idLength > oid2->idLength) return 1;

    return SnmpUtilOidNCmp(oid1, oid2, oid1->idLength);
}

/***********************************************************************
 *      SnmpUtilVarBindCpy      (SNMPAPI.@)
 */
INT WINAPI SnmpUtilVarBindCpy(SnmpVarBind *dst, SnmpVarBind *src)
{
    unsigned int i, size;

    TRACE("(%p, %p)\n", dst, src);

    if (!dst) return SNMPAPI_ERROR;
    if (!src)
    {
        dst->value.asnType = ASN_NULL;
        return SNMPAPI_NOERROR;
    }

    size = src->name.idLength * sizeof(UINT);
    if (!(dst->name.ids = HeapAlloc(GetProcessHeap(), 0, size))) return SNMPAPI_ERROR;

    for (i = 0; i < src->name.idLength; i++) dst->name.ids[i] = src->name.ids[i];
    dst->name.idLength = src->name.idLength;

    if (!asn_any_copy(&dst->value, &src->value))
    {
        HeapFree(GetProcessHeap(), 0, dst->name.ids);
        return SNMPAPI_ERROR;
    }
    return SNMPAPI_NOERROR;
}

/***********************************************************************
 *      SnmpUtilVarBindFree     (SNMPAPI.@)
 */
void WINAPI SnmpUtilVarBindFree(SnmpVarBind *vb)
{
    TRACE("(%p)\n", vb);

    if (!vb) return;

    asn_any_free(&vb->value);
    HeapFree(GetProcessHeap(), 0, vb->name.ids);
    vb->name.idLength = 0;
    vb->name.ids = NULL;
}

/***********************************************************************
 *      SnmpUtilVarBindListCpy  (SNMPAPI.@)
 */
INT WINAPI SnmpUtilVarBindListCpy(SnmpVarBindList *dst, SnmpVarBindList *src)
{
    unsigned int i, size;
    SnmpVarBind *src_entry, *dst_entry;

    TRACE("(%p, %p)\n", dst, src);

    size = src->len * sizeof(SnmpVarBind *);
    if (!(dst->list = HeapAlloc(GetProcessHeap(), 0, size)))
    {
        HeapFree(GetProcessHeap(), 0, dst);
        return SNMPAPI_ERROR;
    }
    src_entry = src->list;
    dst_entry = dst->list;
    for (i = 0; i < src->len; i++)
    {
        if (SnmpUtilVarBindCpy(dst_entry, src_entry))
        {
            src_entry++;
            dst_entry++;
        }
        else
        {
            for (--i; i > 0; i--) SnmpUtilVarBindFree(--dst_entry);
            HeapFree(GetProcessHeap(), 0, dst->list);
            return SNMPAPI_ERROR;
        }
    }
    dst->len = src->len;
    return SNMPAPI_NOERROR;
}

/***********************************************************************
 *      SnmpUtilVarBindListFree (SNMPAPI.@)
 */
void WINAPI SnmpUtilVarBindListFree(SnmpVarBindList *vb)
{
    unsigned int i;
    SnmpVarBind *entry;

    TRACE("(%p)\n", vb);

    entry = vb->list;
    for (i = 0; i < vb->len; i++) SnmpUtilVarBindFree(entry++);
    HeapFree(GetProcessHeap(), 0, vb->list);
}
