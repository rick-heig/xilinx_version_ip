/*  plversion.c - Kernel Module that can get version info from the PL
 *
 *  Copyright (C) 2020 REDS, Rick Wertenbroek <rick.wertenbroek@heig-vd.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS, Rick Wertenbroek");
MODULE_DESCRIPTION("plversion - Provides attributes in the sysfs with PL version information");

#define DRIVER_NAME "plversion"

struct plversion_local {
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
};

static ssize_t plversion_datecode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int len;
        struct plversion_local *lp = dev_get_drvdata(dev);
        uint32_t datecode = ioread32(lp->base_addr + 0x0);
        
        // Format is YYYYMMDD
        uint32_t year = datecode >> 16 & 0xFFFF;
        uint32_t month = datecode >> 8 & 0xFF;
        uint32_t day = datecode & 0xFF;
        len = sprintf(buf, "Date code [YYYY MM DD] : %04x %02x %02x\n", year, month, day);
        return len;
}

static ssize_t plversion_timecode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int len;
        struct plversion_local *lp = dev_get_drvdata(dev);
        uint32_t timecode = ioread32(lp->base_addr + 0x4);

        // Format is HHMMSSuu (u is unused)
        uint32_t hours = timecode >> 24 & 0xFF;
        uint32_t minutes = timecode >> 16 & 0xFF;
        uint32_t seconds = timecode >> 8 & 0xFF;
        len = sprintf(buf, "Time code [HH MM SS] : %02x %02x %02x\n", hours, minutes, seconds);
        return len;
}

static ssize_t plversion_hashcode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int len;
        struct plversion_local *lp = dev_get_drvdata(dev);

        len = sprintf(buf, "%08x\n", ioread32(lp->base_addr + 0x8));
        return len;
}

static ssize_t plversion_versioncode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int len;
        struct plversion_local *lp = dev_get_drvdata(dev);

        len = sprintf(buf, "%08x\n", ioread32(lp->base_addr + 0xC));
        return len;
}

// Create the device attribute structures
static DEVICE_ATTR(datecode, S_IRUGO, plversion_datecode_show, NULL);
static DEVICE_ATTR(timecode, S_IRUGO, plversion_timecode_show, NULL);
static DEVICE_ATTR(hashcode, S_IRUGO, plversion_hashcode_show, NULL);
static DEVICE_ATTR(versioncode, S_IRUGO, plversion_versioncode_show, NULL);

// Group the device attributes
static struct attribute *plversion_attrs[] = {
    &dev_attr_datecode.attr,
    &dev_attr_timecode.attr,
    &dev_attr_hashcode.attr,
    &dev_attr_versioncode.attr,
    NULL
};
ATTRIBUTE_GROUPS(plversion);

// Probe function (can handle multiple pl version devices ...)
static int plversion_probe(struct platform_device *pdev)
{
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct plversion_local *lp = NULL;

	int rc = 0;
	dev_info(dev, "Device Tree Probing plversion device\n");
	/* Get iospace for the device */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}
	lp = (struct plversion_local *) kmalloc(sizeof(struct plversion_local), GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Cound not allocate plversion device\n");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lp);
	lp->mem_start = r_mem->start;
	lp->mem_end = r_mem->end;

        // The memory region does not need to be locked
	//if (!request_mem_region(lp->mem_start,
	//			lp->mem_end - lp->mem_start + 1,
	//			DRIVER_NAME)) {
	//	dev_err(dev, "Couldn't lock memory region at %p\n",
	//		(void *)lp->mem_start);
	//	rc = -EBUSY;
	//	goto error1;
	//}

	lp->base_addr = ioremap(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	if (!lp->base_addr) {
		dev_err(dev, "plversion: Could not allocate iomem\n");
		rc = -EIO;
		goto error2;
	}

        // Sysfs entry
        rc = sysfs_create_group(&dev->kobj, &plversion_group);
        if (rc) {
            dev_err(dev, "sysfs creation failed\n");
            return rc;
        }

        dev_info(dev, "date code : 0x%08x\n", ioread32(lp->base_addr + 0x0));
        dev_info(dev, "time code : 0x%08x\n", ioread32(lp->base_addr + 0x4));
        dev_info(dev, "hash code : 0x%08x\n", ioread32(lp->base_addr + 0x8));
        dev_info(dev, "ver. code : 0x%08x\n", ioread32(lp->base_addr + 0xC));
        
	return 0;

error2:
	//release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
        ;
error1:
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return rc;
}

// Remove function
static int plversion_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plversion_local *lp = dev_get_drvdata(dev);
        sysfs_remove_group(&dev->kobj, &plversion_group);
	iounmap(lp->base_addr);
	//release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}

// Device tree compatible strings
#ifdef CONFIG_OF
static struct of_device_id plversion_of_match[] = {
        { .compatible = "reds,plversion", }, /* User defined name */
        { .compatible = "xlnx,version-ip-1.0", }, /* Auto-generated name */
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plversion_of_match);
#else
# define plversion_of_match
#endif

static struct platform_driver plversion_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= plversion_of_match,
	},
	.probe		= plversion_probe,
	.remove		= plversion_remove,
};

static int __init plversion_init(void)
{
	return platform_driver_register(&plversion_driver);
}


static void __exit plversion_exit(void)
{
	platform_driver_unregister(&plversion_driver);
}

module_init(plversion_init);
module_exit(plversion_exit);
