/*
 * tlp-dualfs.c
 *
 * TALPA Filesystem Interceptor
 *
 * Copyright (C) 2004-2011 Sophos Limited, Oxford, England.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License Version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <asm/errno.h>

#include "tlp-test.h"


#include "common/bool.h"
#define TALPA_SUBSYS "dualfstest"
#include "common/talpa.h"

#include "components/services/configurator_impl/dualfs_configurator.h"

#define CFGDATASIZE      (sizeof(char) * 16)

typedef struct {
    char    name[CFGDATASIZE];
    char    value[CFGDATASIZE];
} DualfsTestConfigData;

typedef struct tag_DualfsTest
{
    IConfigurable               i_IConfigurable;
    void                        (*delete)(struct tag_DualfsTest* object);
    bool                        mEnabled;
    PODConfigurationElement     mConfig[2];
    DualfsTestConfigData        mConfigData[1];
} DualfsTest;

static bool enable(void* self);
static void disable(void* self);
static const char* configName(const void* self);
static const PODConfigurationElement* allConfig(const void* self);
static const char* config(const void* self, const char* name);
static void setConfig(void* self, const char* name, const char* value);
static void deleteDualfsTest(struct tag_DualfsTest* object);

#define CFG_STATUS          "status"
#define CFG_VALUE_ENABLED   "enabled"
#define CFG_VALUE_DISABLED  "disabled"
#define CFG_ACTION_ENABLE   "enable"
#define CFG_ACTION_DISABLE  "disable"

static DualfsTest template_DualfsTest =
    {
        {
            configName,
            allConfig,
            config,
            setConfig,
            0,
            (void (*)(void*))deleteDualfsTest
        },
        deleteDualfsTest,
        true,
        {
            {0, 0, CFGDATASIZE, true },
            {0, 0, 0, false }
        },
        {
            { CFG_STATUS, CFG_VALUE_ENABLED }
        }

    };
#define this    ((DualfsTest*)self)

static DualfsConfigurator*  mConfig;
static DualfsTest*          mDualfsTest;


DualfsTest* newDualfsTest(void)
{
    DualfsTest* object;


    object = kmalloc(sizeof(template_DualfsTest), GFP_KERNEL);
    if (object != 0)
    {
        memcpy(object, &template_DualfsTest, sizeof(template_DualfsTest));
        object->i_IConfigurable.object = object;
        object->mConfig[0].name  = object->mConfigData[0].name;
        object->mConfig[0].value = object->mConfigData[0].value;
    }
    return object;
}

static void deleteDualfsTest(struct tag_DualfsTest* object)
{
    if (object != 0)
    {
        kfree(object);
    }
    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long talpa_ioctl(struct file *file, unsigned int cmd, unsigned long parm)
#else
int talpa_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long parm)
#endif
{
    int ret = -ENOTTY;

    switch ( cmd )
    {
    }

    return ret;
}

struct file_operations talpa_fops =
{
    owner:  THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    unlocked_ioctl:  talpa_ioctl
#else
    ioctl:  talpa_ioctl
#endif
};

static int __init talpa_test_init(void)
{
    int ret;

    mConfig = newDualfsConfigurator(false);
    if ( !mConfig )
    {
        err("Failed to allocate configurator");
        return -ENOMEM;
    }

    mDualfsTest = newDualfsTest();
    if ( !mDualfsTest )
    {
        mConfig->delete(mConfig);
        err("Failed to allocate dualfs test");
        return -ENOMEM;
    }

    ret = mConfig->i_IConfigurator.attach(mConfig, ECG_InterceptProcessor, &mDualfsTest->i_IConfigurable);

    if ( ret )
    {
        err("Failed to attach configuration!");
        mDualfsTest->delete(mDualfsTest);
        mConfig->delete(mConfig);
        return ret;
    }

    ret = register_chrdev(TALPA_MAJOR, TALPA_DEVICE, &talpa_fops);

    if ( ret )
    {
        err("Failed to register TALPA Test character device!");
        mConfig->i_IConfigurator.detach(mConfig, &mDualfsTest->i_IConfigurable);
        mDualfsTest->delete(mDualfsTest);
        mConfig->delete(mConfig);
        return ret;
    }

    return 0;
}

static void __exit talpa_test_exit(void)
{
    int ret;

    mConfig->i_IConfigurator.detach(mConfig, &mDualfsTest->i_IConfigurable);
    mDualfsTest->delete(mDualfsTest);
    mConfig->delete(mConfig);

    ret = talpa_unregister_chrdev(TALPA_MAJOR, TALPA_DEVICE);

    if ( ret )
    {
        err("Hmmmmmm... very strange things are happening!");
    }
}

static bool enable(void* self)
{
    if (!this->mEnabled)
    {
        this->mEnabled = true;
        strcpy(this->mConfig[0].value, CFG_VALUE_ENABLED);
        info("Filter enabled.");
    }
    return true;
}

static void disable(void* self)
{
    if (this->mEnabled)
    {
        this->mEnabled = false;
        strcpy(this->mConfig[0].value, CFG_VALUE_DISABLED);
        info("Filter disabled.");
    }
    return;
}

/*
 * IConfigurable.
 */
static const char* configName(const void* self)
{
    return "DualfsTest";
}

static const PODConfigurationElement* allConfig(const void* self)
{
    return this->mConfig;
}

static const char* config(const void* self, const char* name)
{
    PODConfigurationElement*    cfgElement;


    /*
     * Find the named item.
     */
    for (cfgElement = this->mConfig; cfgElement->name != 0; cfgElement++)
    {
        if (strcmp(name, cfgElement->name) == 0)
        {
            break;
        }
    }

    /*
     * Return what was found else a null pointer.
     */
    if (cfgElement->name != 0)
    {
        return cfgElement->value;
    }
    return 0;
}

static void  setConfig(void* self, const char* name, const char* value)
{
   PODConfigurationElement*    cfgElement;


    /*
     * Find the named item.
     */
    for (cfgElement = this->mConfig; cfgElement->name != 0; cfgElement++)
    {
        if (strcmp(name, cfgElement->name) == 0)
        {
            break;
        }
    }

    /*
     * Cant set that which does not exist!
     */
    if (cfgElement->name == 0)
    {
        return;
    }

    /*
     * OK time to do some work...
     */
    if (strcmp(name, CFG_STATUS) == 0)
    {
        if (strcmp(value, CFG_ACTION_ENABLE) == 0)
        {
            enable(this);
        }
        else if (strcmp(value, CFG_ACTION_DISABLE) == 0)
        {
            disable(this);
        }
    }
    return;
}

/*
 *
 * Module information.
 *
 */
MODULE_AUTHOR("Sophos Limited");
MODULE_DESCRIPTION("TALPA Filesystem Interceptor Test Module");
MODULE_LICENSE("GPL");

module_init(talpa_test_init);
module_exit(talpa_test_exit);

