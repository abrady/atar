/* This is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <sys/stat.h>

typedef unsigned U32;
typedef unsigned char U8;
#define BOOL int
#define TRUE 1
#define FALSE (!TRUE)

#define TAR_HDR_SIZE 512

typedef struct TarHdr
{
    char fname[100];
    U32 file_size;
    U32 last_mod_time;    
} TarHdr;

typedef struct Tar
{
    U8 *start;
    U8 *data;
    U32 len;
    TarHdr hdr;
} Tar;

Tar *tar_from_data(U8 *data, U32 len)
{
    Tar *t;
    if(!data)
        return NULL;
    t = (Tar*)calloc(sizeof(Tar),1);
    t->start = data;
    t->data = data;
    t->len = len;
    return t;
}

void Tar_Destroy(Tar *t)
{
    free(t);
}

// only grabs the filename, file size.
BOOL tar_process_cur(Tar *t)
{
    char *data;
    if(!t)
        return FALSE;
    data = t->data;
    strncpy(t->hdr.fname,data,100);
    data+=100;
    data+=24; // skip mode, owner and user id
    if(1!=sscanf(data,"%o",&t->hdr.file_size))
        return FALSE;
    
    data = t->data + 257;
    if(data[0] != 'u' || data[1] != 's' || data[2] != 't' || data[3] != 'a' || data[4] != 'r')
        return FALSE;
    return TRUE;
}

void *tar_cur_file(Tar *t, U32 *len)
{
    if(!tar_process_cur(t))
        return NULL;
    if(len)
        *len = t->hdr.file_size;
    return t->data + TAR_HDR_SIZE;
}

// must free returned value
void *tar_alloc_cur_file(Tar *t, U32 *len)
{
    char *s;
    char *d;
    int n;
    
    if(!tar_process_cur(t))
        return NULL;
    n = t->hdr.file_size;
    if(len)
        *len = n;
    d = (char*)malloc(n);
    s = t->data + TAR_HDR_SIZE;
    memcpy(d,s,n);
    return d;
}

BOOL tar_next(Tar *t)
{
    if(!tar_process_cur(t))
        return FALSE;
    t->data += ((TAR_HDR_SIZE + t->hdr.file_size + 511) & ~511);
    if(t->data >= t->start + t->len)
        return FALSE;
    return tar_process_cur(t);
}


// *************************************************************************
// test area
// *************************************************************************
intptr_t file_size(char *fname){
	struct _stat32 status;
    if(!_stat32(fname, &status) && status.st_mode & _S_IFREG)
        return status.st_size;
    // The file doesn't exist.
    return -1;
}

void *file_alloc(char *fname, int *pn)
{
    void *b;
    int n;
    FILE *fp;
    n = file_size(fname);
    if(n<0)
        return NULL;
    if(pn)
        *pn = n;
    b = (void*)malloc(n+1);
    fp = fopen(fname,"rb");
    if(fread(b,1,n,fp) < n)
    {
        free(b);
        b = NULL;
    }
    fclose(fp);
    return b;
}


int main(int argc, char **argv)
{
    char *src,*srcf = "foo.tar"; int srcn;
    char *f1,*f1t,*f1f = "temp.in.txt"; int f1n, f1nt;
    char *f2,*f2t,*f2f = "temp.out.txt"; int f2n, f2nt;
    FILE *fp;
    Tar *t;
    src = file_alloc(srcf,&srcn);
    t = tar_from_data(src,srcn);

    printf("testing tar\n");
#define TST(COND,MSG)                                   \
    if(!(COND)){                                        \
        printf("error " MSG " cond " #COND "\n");       \
        exit(1);                                        \
    }
        
    f1 = file_alloc(f1f,&f1n);
    f1t = tar_alloc_cur_file(t,&f1nt);
    TST(0==strcmp(f1f,t->hdr.fname),"not the same filename")
    TST(f1n == f1nt,"f1 lengths not equal");
    TST(0==memcmp(f1,f1t,f1n),"f1 memory not the same");
    free(f1);
    free(f1t);

    TST(tar_next(t),"couldn't move to second archive'd file");
    f2 = file_alloc(f2f,&f2n);
    f2t = tar_cur_file(t,&f2nt);
    TST(0==strcmp(f2f,t->hdr.fname),"not the same filename")
    TST(f2n == f2nt,"f2 lengths not equal");
    TST(0==memcmp(f2,f2t,f2n),"f2 memory not the same");
    free(f2);
    
    TST(!tar_next(t),"able to move to third archive, should be EOF");    
    Tar_Destroy(t);
    printf("done\n");
}
