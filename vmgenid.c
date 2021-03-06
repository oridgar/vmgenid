/*
 * Virtual Machine Generation ID device
 *
 * Copyright (C) 2018 Red Hat, Inc. All rights reserved.
 *	Authors:
 *	  Or Idgar <oridgar@gmail.com>
 *	  Gal Hammer <ghammer@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/uuid.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Or Idgar <oridgar@gmail.com>");
MODULE_AUTHOR("Gal Hammer <ghammer@redhat.com>");
MODULE_DESCRIPTION("Virtual Machine Generation ID");
MODULE_VERSION("0.1");

ACPI_MODULE_NAME("vmgenid");

static u64 phy_addr;

static ssize_t generation_id_show(struct device *_d,
			      struct device_attribute *attr, char *buf)
{
	uuid_t *uuidp;
	ssize_t result;

	uuidp = acpi_os_map_iomem(phy_addr, sizeof(uuid_t));
	if (!uuidp)
		return -EFAULT;

	result = sprintf(buf, "%pUl\n", uuidp);
	acpi_os_unmap_iomem(uuidp, sizeof(uuid_t));
	return result;
}
static DEVICE_ATTR_RO(generation_id);

static ssize_t raw_show(struct device *_d,
			struct device_attribute *attr,
			      char *buf)
{
	uuid_t *uuidp;

	uuidp = acpi_os_map_iomem(phy_addr, sizeof(uuid_t));
	if (!uuidp)
		return -EFAULT;
	memcpy(buf, uuidp, sizeof(uuid_t));
	acpi_os_unmap_iomem(uuidp, sizeof(uuid_t));
	return sizeof(uuid_t);
}
static DEVICE_ATTR_RO(raw);

static struct attribute *vmgenid_dev_attrs[] = {
	&dev_attr_generation_id.attr,
	&dev_attr_raw.attr,
	NULL,
};
ATTRIBUTE_GROUPS(vmgenid_dev);

static int get_vmgenid(acpi_handle handle)
{
	int i;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;
	union acpi_object *pss;
	union acpi_object *element;

	status = acpi_evaluate_object(handle, "ADDR", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Evaluating _ADDR"));
		return -ENODEV;
	}
	pss = buffer.pointer;
	if (!pss || pss->type != ACPI_TYPE_PACKAGE || pss->package.count != 2)
		return -EFAULT;

	phy_addr = 0;
	for (i = 0; i < pss->package.count; i++) {
		element = &(pss->package.elements[i]);
		if (element->type != ACPI_TYPE_INTEGER)
			return -EFAULT;
		phy_addr |= element->integer.value << i*32;
	}
	return 0;
}

static int acpi_vmgenid_add(struct acpi_device *device)
{
	int error;

	if (!device)
		return -EINVAL;
	error = sysfs_create_group(&(device->dev.kobj), *vmgenid_dev_groups);
	if (error)
		return error;
	return get_vmgenid(device->handle);
}

static int acpi_vmgenid_remove(struct acpi_device *device)
{
	sysfs_remove_group(&device->dev.kobj, *vmgenid_dev_groups);
	return 0;
}

static void acpi_vmgenid_notify(struct acpi_device *device, u32 event)
{
	get_vmgenid(device->handle);
}

static const struct acpi_device_id vmgenid_ids[] = {
	{"QEMUVGID", 0},
	{"", 0},
};

static struct acpi_driver acpi_vmgenid_driver = {
	.name = "vm_gen_counter",
	.ids = vmgenid_ids,
	.owner = THIS_MODULE,
	.ops = {
		.notify = acpi_vmgenid_notify,
		.add = acpi_vmgenid_add,
		.remove = acpi_vmgenid_remove,
	}
};

static int __init vmgenid_init(void)
{
	return acpi_bus_register_driver(&acpi_vmgenid_driver);
}

static void __exit vmgenid_exit(void)
{
	acpi_bus_unregister_driver(&acpi_vmgenid_driver);
}

module_init(vmgenid_init);
module_exit(vmgenid_exit);
