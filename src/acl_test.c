/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This prog is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This prog is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this prog with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this prog; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

/*
 * Test our various libacl functions.
 * Use IRIX semantics or Linux semantics if pertinent.
 */
 
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/acl.h>

char *prog;
int irixsemantics = 0;

void usage(void)
{
    fprintf(stderr, "usage: %s\n"
           "    -i - use irix semantics\n" 
           ,prog);
           
}

void
print_err(char *msg)
{
    printf("%s: %s: %s\n", prog, msg, strerror(errno));
}

void
dump_ace(acl_entry_t ace)
{
    printf("<tag:%d,id:%d,perm:%u>", 
	    ace->ae_tag, ace->ae_id, ace->ae_perm);
}

void
dump_acl(acl_t acl)
{
    int i;
    printf("ACL[n=%d]: ", acl->acl_cnt);
    for (i=0;i<acl->acl_cnt;i++) {
	acl_entry_t ace = &acl->acl_entry[i];
	printf("%d: ", i);
	dump_ace(ace);
	printf(" ");
    }
    printf("\n");

}

void
dump_acl_by_entry(acl_t acl)
{
    int sts, i;
    acl_entry_t ace; 

    printf("Get 1st entry on filled ACL\n");
    sts = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace);
    printf("acl_get_entry -> %d\n", sts);
    if (sts > 0) {
	printf("1: "); dump_ace(ace); printf("\n");
    }

    for(i=2;i<=acl->acl_cnt+2;i++) {
	printf("Get %dth entry on filled ACL\n", i);
	sts = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace);
	printf("acl_get_entry -> %d\n", sts);
	if (sts > 0) {
	    printf("%d: ",i); dump_ace(ace); printf("\n");
	}
    }
}

/*
 * create a full acl with entries with known bogus values
 */
acl_t
create_filled_acl(void)
{
    acl_t acl;
    int i;

    acl = acl_init(ACL_MAX_ENTRIES);	

    for(i=0;i<ACL_MAX_ENTRIES;i++) {
	acl_entry_t ace = &acl->acl_entry[i];
	ace->ae_tag = i;
	ace->ae_id = i+1;
	ace->ae_perm = i+2;
	acl->acl_cnt++;
    }
    return acl;
}

void
test_acl_get_qualifier(void)
{
    struct acl_entry ace;
    uid_t *uidp;

    printf("*** test out acl_get_qualifier ***\n");

    /* simple ace */
    ace.ae_tag = ACL_USER;
    ace.ae_id = 1;
    ace.ae_perm = 1;

    /* make sure we can get uid and free it */
    uidp = acl_get_qualifier(&ace); 
    printf("uid = %d\n", *uidp);
    acl_free(uidp);

    /* change to another valid tag with a qualifier */
    ace.ae_tag = ACL_GROUP;
    uidp = acl_get_qualifier(&ace); 
    printf("uid = %d\n", *uidp);
    acl_free(uidp);

    /* let's get some errors */

    ace.ae_tag = ACL_USER_OBJ;
    uidp = acl_get_qualifier(&ace); 
    if (uidp == NULL)
	printf("uidp is NULL: %s\n", strerror(errno));
    else
	printf("Error: uidp is NOT NULL\n");

    uidp = acl_get_qualifier(NULL); 
    if (uidp == NULL)
	printf("uidp is NULL: %s\n", strerror(errno));
    else
	printf("Error: uidp is NOT NULL\n");
}

int
main(int argc, char **argv)
{
	int c, i;
	acl_t acl1, acl2, acl3;
	acl_entry_t ace1;

        prog = basename(argv[0]);

	while ((c = getopt(argc, argv, "i")) != -1) {
		switch (c) {
		case 'i':
			irixsemantics = 1;
			break;
		case '?':
                        usage();
			return 1;
		}
	}

	if (irixsemantics) {
	    acl_set_compat(ACL_COMPAT_IRIXGET);
	}

        /* ---------------------------------------------- */
        printf("*** test out creating an ACL ***\n");

	printf("Test acl_init(ACL_MAX_ENTRIES+1)\n");
	acl1 = acl_init(ACL_MAX_ENTRIES+1);
	if (acl1 == NULL) {
	    print_err("acl_init(max+1)");
	}
	printf("Test acl_init(-1)\n");
	acl1 = acl_init(-1);
	if (acl1 == NULL) {
	    print_err("acl_init(-1)");
	}
	printf("Test acl_init(0)\n");
	acl1 = acl_init(0);
	if (acl1 == NULL) {
	    print_err("acl_init(0)");
	}

	printf("Test acl_create_entry(NULL, ...)\n");
	if (acl_create_entry(NULL, &ace1) == -1) {
	    print_err("acl_create_entry(NULL,ace1)");
	}
	printf("Test acl_create_entry(..., NULL)\n");
	if (acl_create_entry(&acl1, NULL) == -1) {
	    print_err("acl_create_entry(NULL,ace1)");
	}
	printf("Test acl_create_entry(acl1, ace1)\n");
	acl1 = NULL;
	if (acl_create_entry(&acl1, &ace1) == -1) {
	    print_err("acl_create_entry(*null,ace1)");
	}

	acl_free(acl1);
	acl1 = acl_init(0);
	for (i=0;i<=ACL_MAX_ENTRIES+1;i++) {
	    printf("%d: creating ace\n", i);
	    if (acl_create_entry(&acl1, &ace1) == -1) {
		print_err("acl_create_entry");
	    }
	    dump_acl(acl1);
	}

        /* ---------------------------------------------- */
        printf("*** test out getting ACEs ***\n");

	dump_acl_by_entry(acl1);

	printf("dump empty ACL\n");
	acl2 = acl_init(0);
	if (acl2 == NULL) {
	    print_err("acl_init(0)");
	}
	dump_acl_by_entry(acl2);

	printf("fill an ACL with known bogus values\n");
	acl3 = create_filled_acl();	
	dump_acl_by_entry(acl3);

        /* ---------------------------------------------- */
        printf("*** test out ACL to text for empty ACL***\n");
	{
	    char *text;
	    ssize_t len;
	    acl_t empty_acl = acl_init(0);
	    text = acl_to_text(empty_acl, NULL); 
            printf("acl_to_text(empty_acl,NULL) -> \"%s\"\n", text); 
	    text = acl_to_text(empty_acl, &len); 
            printf("acl_to_text(empty_acl,NULL) -> \"%s\", len = %u\n", text, len); 
	    text = acl_to_text(NULL, NULL); 
            printf("acl_to_text(NULL,NULL) -> \"%s\"\n", text==NULL?"NULL":text); 
        }
	/* NOTE: Other tests will test out the text for ACLs with ACEs.
         *       So don't have to test it here.
         *       It is simplest to choose ids not in /etc/passwd /etc/group
         *       which is done already in a script. 
         */

	test_acl_get_qualifier();

	return 0;
}
