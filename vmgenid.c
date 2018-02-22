// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/uuid.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Or Idgar <oridgar@gmail.com>");
MODULE_AUTHOR("Gal Hammer <ghammer@redhat.com>");
MODULE_DESCRIPTION("Virtual Machine Generation ID");
MODULE_VERSION("0.1");

ACPI_MODULE_NAME("vmgenid");

static struct kobject *vmgenid_kobj;
static u64 phy_addr;

static ssize_t sysfs_vmgenid_str_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
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

static ssize_t sysfs_vmgenid_raw_show(struct kobject *kobj,
			      struct kobj_attribute *attr,
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
	return get_vmgenid(device->handle);
}

static int acpi_vmgenid_remove(struct acpi_device *device)
{
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

static struct kobj_attribute vmgenid_attribute =
	__ATTR(generation_id, 0440, sysfs_vmgenid_str_show, NULL);
static struct kobj_attribute vmgenid_raw_attr =
	__ATTR(raw, 0440, sysfs_vmgenid_raw_show, NULL);

static int __init vmgenid_init(void)
{
	int error = 0;
	vmgenid_kobj = kobject_create_and_add("vm_gen_counter", kernel_kobj);
	if (!vmgenid_kobj)
		return -ENOMEM;
	error = sysfs_create_file(vmgenid_kobj, &vmgenid_attribute.attr);
	if (error)
		return -EFAULT;
	error = sysfs_create_file(vmgenid_kobj, &vmgenid_raw_attr.attr);
	if (error)
		return -EFAULT;
	error = acpi_bus_register_driver(&acpi_vmgenid_driver);
	if (error < 0)
		return error;
	return 0;
}

static void __exit vmgenid_exit(void)
{
	kobject_put(vmgenid_kobj);
	acpi_bus_unregister_driver(&acpi_vmgenid_driver);
}

module_init(vmgenid_init);
module_exit(vmgenid_exit);
