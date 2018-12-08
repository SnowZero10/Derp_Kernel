/*
 * productinfo.c
 *
 * Disp device info!
 *
 * Copyright (C) HMCT Corporation 2013
 *
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/export.h>
#include <linux/productinfo.h>

#define PRODUCTINFO_BUFF_LEN  100

const char *deviceclassname[PRODUCTINFO_MAX_ID]={
"Vendor",
"Board and product",
"Hardware version",
"LCD",
"CTP",
"HDMI",
"Main camera",
"Front camera",
"DDR",
"EMMC",
"NAND",
"Accelerometer sensor",
"Compass sensor",
"Alps sensor",
"BT",
"WIFI",
"Codec",
"Modem",
"LED",
};

typedef struct productinfo_struct {
    int     used;
    char  productinfo_data[PRODUCTINFO_BUFF_LEN];
}productinfo_type;

productinfo_type productinfo_data[PRODUCTINFO_MAX_ID];
char *productinfo_data_ptr;


int productinfo_register(int id, const char *devicename, const char *deviceinfo)
{
    int len = 0;
    if(id >= PRODUCTINFO_MAX_ID)
    {
        return -ENOMEM;
    }
    if(!deviceclassname[id])
    {
        return -ENOMEM;
    }
    len = strlen(deviceclassname[id]);
    if(devicename)
    {
        len += strlen(devicename);
    }
    if(deviceinfo)
    {
        len += strlen(deviceinfo);
    }
    if(len >= PRODUCTINFO_BUFF_LEN - 5)
    {
        return -ENOMEM;
    }
    memset(productinfo_data[id].productinfo_data,0,sizeof(productinfo_data[id].productinfo_data));
    productinfo_data_ptr = productinfo_data[id].productinfo_data;
    productinfo_data[id].used = 1;
    strcat(productinfo_data_ptr, deviceclassname[id]);
    if(devicename)
    {
        strcat(productinfo_data_ptr,": ");
        strcat(productinfo_data_ptr, devicename);
    }
    if(deviceinfo)
    {
        strcat(productinfo_data_ptr,"--");
        strcat(productinfo_data_ptr, deviceinfo);
    }
    strcat(productinfo_data_ptr,"\n");
    return 0;
}
EXPORT_SYMBOL(productinfo_register);

static int productinfo_proc_show(struct seq_file *m, void *v)
{
    int i;
    for(i=0; i<PRODUCTINFO_MAX_ID; i++)
    {
        if(productinfo_data[i].used)
        {
            productinfo_data_ptr = productinfo_data[i].productinfo_data;
            seq_write(m, productinfo_data_ptr, strlen(productinfo_data_ptr));
        }
    }
    return 0;
}


static int productinfo_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, productinfo_proc_show, NULL);
}

static const struct file_operations productinfo_proc_fops = {
    .open       = productinfo_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int __init proc_productinfo_init(void)
{
    proc_create("productinfo", 0, NULL, &productinfo_proc_fops);
#if CONFIG_MACH_HISENSE_SMARTPHONE
    productinfo_register(PRODUCTINFO_VENDOR_ID, CONFIG_VENDOR_INFO, NULL);
    productinfo_register(PRODUCTINFO_BOARD_ID, CONFIG_BOARD_INFO, CONFIG_PRODUCT_INFO);
    //productinfo_register(PRODUCTINFO_HW_VERSION_ID, CONFIG_HARDWARE_VERSION_INFO, NULL);
#endif
    return 0;
}
module_init(proc_productinfo_init);
