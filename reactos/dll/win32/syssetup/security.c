/*
 * COPYRIGHT:         See COPYING in the top level directory
 * PROJECT:           ReactOS system libraries
 * PURPOSE:           System setup
 * FILE:              dll/win32/syssetup/security.c
 * PROGRAMER:         Eric Kohl
 */

/* INCLUDES *****************************************************************/

#include "precomp.h"

#define NDEBUG
#include <debug.h>


/* FUNCTIONS ****************************************************************/

NTSTATUS
SetAccountDomain(LPCWSTR DomainName,
                 PSID DomainSid)
{
    PPOLICY_ACCOUNT_DOMAIN_INFO OrigInfo = NULL;
    POLICY_ACCOUNT_DOMAIN_INFO Info;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes;
    LSA_HANDLE PolicyHandle;
    NTSTATUS Status;

    memset(&ObjectAttributes, 0, sizeof(LSA_OBJECT_ATTRIBUTES));
    ObjectAttributes.Length = sizeof(LSA_OBJECT_ATTRIBUTES);

    Status = LsaOpenPolicy(NULL,
                           &ObjectAttributes,
                           POLICY_TRUST_ADMIN,
                           &PolicyHandle);
    if (Status != STATUS_SUCCESS)
    {
        DPRINT("LsaOpenPolicy failed (Status: 0x%08lx)\n", Status);
        return Status;
    }

    Status = LsaQueryInformationPolicy(PolicyHandle,
                                       PolicyAccountDomainInformation,
                                       (PVOID *)&OrigInfo);
    if (Status == STATUS_SUCCESS && OrigInfo != NULL)
    {
        if (DomainName == NULL)
        {
            Info.DomainName.Buffer = OrigInfo->DomainName.Buffer;
            Info.DomainName.Length = OrigInfo->DomainName.Length;
            Info.DomainName.MaximumLength = OrigInfo->DomainName.MaximumLength;
        }
        else
        {
            Info.DomainName.Buffer = (LPWSTR)DomainName;
            Info.DomainName.Length = wcslen(DomainName) * sizeof(WCHAR);
            Info.DomainName.MaximumLength = Info.DomainName.Length + sizeof(WCHAR);
        }

        if (DomainSid == NULL)
            Info.DomainSid = OrigInfo->DomainSid;
        else
            Info.DomainSid = DomainSid;
    }
    else
    {
        Info.DomainName.Buffer = (LPWSTR)DomainName;
        Info.DomainName.Length = wcslen(DomainName) * sizeof(WCHAR);
        Info.DomainName.MaximumLength = Info.DomainName.Length + sizeof(WCHAR);
        Info.DomainSid = DomainSid;
    }

    Status = LsaSetInformationPolicy(PolicyHandle,
                                     PolicyAccountDomainInformation,
                                     (PVOID)&Info);
    if (Status != STATUS_SUCCESS)
    {
        DPRINT("LsaSetInformationPolicy failed (Status: 0x%08lx)\n", Status);
    }

    if (OrigInfo != NULL)
        LsaFreeMemory(OrigInfo);

    LsaClose(PolicyHandle);

    return Status;
}


static
VOID
InstallBuiltinAccounts(VOID)
{
    LPWSTR BuiltinAccounts[] = {
        L"S-1-1-0",         /* Everyone */
        L"S-1-5-4",         /* Interactive */
        L"S-1-5-6",         /* Service */
        L"S-1-5-19",        /* Local Service */
        L"S-1-5-20",        /* Network Service */
        L"S-1-5-32-544",    /* Administrators */
        L"S-1-5-32-545",    /* Users */
        L"S-1-5-32-547",    /* Power Users */
        L"S-1-5-32-551",    /* Backup Operators */
        L"S-1-5-32-555"};   /* Remote Desktop Users */
    LSA_OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    LSA_HANDLE PolicyHandle = NULL;
    LSA_HANDLE AccountHandle = NULL;
    PSID AccountSid;
    ULONG i;

    DPRINT("InstallBuiltinAccounts()\n");

    memset(&ObjectAttributes, 0, sizeof(LSA_OBJECT_ATTRIBUTES));

    Status = LsaOpenPolicy(NULL,
                           &ObjectAttributes,
                           POLICY_CREATE_ACCOUNT,
                           &PolicyHandle);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("LsaOpenPolicy failed (Status %08lx)\n", Status);
        return;
    }

    for (i = 0; i < 10; i++)
    {
        ConvertStringSidToSid(BuiltinAccounts[i], &AccountSid);

        Status = LsaCreateAccount(PolicyHandle,
                                  AccountSid,
                                  0,
                                  &AccountHandle);
        if (NT_SUCCESS(Status))
        {
            LsaClose(AccountHandle);
        }

        LocalFree(AccountSid);
    }

    LsaClose(PolicyHandle);
}


static
VOID
InstallPrivileges(VOID)
{
    HINF hSecurityInf = INVALID_HANDLE_VALUE;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes;
    WCHAR szPrivilegeString[256];
    WCHAR szSidString[256];
    INFCONTEXT InfContext;
    DWORD i;
    PRIVILEGE_SET PrivilegeSet;
    PSID AccountSid;
    NTSTATUS Status;
    LSA_HANDLE PolicyHandle = NULL;
    LSA_HANDLE AccountHandle;

    DPRINT("InstallPrivileges()\n");

    hSecurityInf = SetupOpenInfFileW(L"defltws.inf", //szNameBuffer,
                                     NULL,
                                     INF_STYLE_WIN4,
                                     NULL);
    if (hSecurityInf == INVALID_HANDLE_VALUE)
    {
        DPRINT1("SetupOpenInfFileW failed\n");
        return;
    }

    memset(&ObjectAttributes, 0, sizeof(LSA_OBJECT_ATTRIBUTES));

    Status = LsaOpenPolicy(NULL,
                           &ObjectAttributes,
                           POLICY_CREATE_ACCOUNT,
                           &PolicyHandle);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("LsaOpenPolicy failed (Status %08lx)\n", Status);
        goto done;
    }

    if (!SetupFindFirstLineW(hSecurityInf,
                             L"Privilege Rights",
                             NULL,
                             &InfContext))
    {
        DPRINT1("SetupFindfirstLineW failed\n");
        goto done;
    }

    PrivilegeSet.PrivilegeCount = 1;
    PrivilegeSet.Control = 0;

    do
    {
        /* Retrieve the privilege name */
        if (!SetupGetStringFieldW(&InfContext,
                                  0,
                                  szPrivilegeString,
                                  256,
                                  NULL))
        {
            DPRINT1("SetupGetStringFieldW() failed\n");
            goto done;
        }
        DPRINT("Privilege: %S\n", szPrivilegeString);

        if (!LookupPrivilegeValueW(NULL,
                                   szPrivilegeString,
                                   &(PrivilegeSet.Privilege[0].Luid)))
        {
            DPRINT1("LookupPrivilegeNameW() failed\n");
            goto done;
        }

        PrivilegeSet.Privilege[0].Attributes = 0;

        for (i = 0; i < SetupGetFieldCount(&InfContext); i++)
        {
            if (!SetupGetStringFieldW(&InfContext,
                                      i + 1,
                                      szSidString,
                                      256,
                                      NULL))
            {
                DPRINT1("SetupGetStringFieldW() failed\n");
                goto done;
            }
            DPRINT("SID: %S\n", szSidString);

            ConvertStringSidToSid(szSidString, &AccountSid);

            Status = LsaOpenAccount(PolicyHandle,
                                    AccountSid,
                                    ACCOUNT_VIEW | ACCOUNT_ADJUST_PRIVILEGES,
                                    &AccountHandle);
            if (NT_SUCCESS(Status))
            {
                Status = LsaAddPrivilegesToAccount(AccountHandle,
                                                   &PrivilegeSet);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("LsaAddPrivilegesToAccount() failed (Status %08lx)\n", Status);
                }

                LsaClose(AccountHandle);
            }

            LocalFree(AccountSid);
        }

    }
    while (SetupFindNextLine(&InfContext, &InfContext));

done:
    if (PolicyHandle != NULL)
        LsaClose(PolicyHandle);

    if (hSecurityInf != INVALID_HANDLE_VALUE)
        SetupCloseInfFile(hSecurityInf);
}

VOID
InstallSecurity(VOID)
{
    InstallBuiltinAccounts();
    InstallPrivileges();
}