#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/uuid.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Or Idgar");
MODULE_DESCRIPTION("Expose VM Generation ID from ACPI");
MODULE_VERSION("0.1");

static struct kobject *vmgenid_kobj;
static u64 phy_addr;

static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

        uuid_t *uuid_tmp;
	u64 uuid[2];
	u32 *uuid_chunk1;
	u16 *uuid_chunk2;
	u16 *uuid_chunk3;
	u8 *uuid_chunk4;
	int *virtual_addr;

	virtual_addr = acpi_os_map_iomem(phy_addr, sizeof(uuid_t)); 
	uuid_tmp = (uuid_t*)virtual_addr;
	if (!uuid_tmp) {
		printk(KERN_INFO "could not map vmgenid virtual memory!\n");
		return -EFAULT;
	}
	memcpy_fromio(uuid, virtual_addr, sizeof(uuid_t));
	

	uuid_chunk1 = (u32*)uuid;
	uuid_chunk2 = (u16*)(uuid_chunk1 + 1);
	uuid_chunk3 = (u16*)(uuid_chunk2 + 1);
	uuid_chunk4 = (u8*)(uuid_chunk3 + 1);
	acpi_os_unmap_iomem(virtual_addr,sizeof(uuid_t));
	return sprintf(buf, "%x-%x-%x-%x%x-%x%x%x%x%x%x\n", 
		      *uuid_chunk1, *uuid_chunk2, *uuid_chunk3,
		      uuid_chunk4[0],uuid_chunk4[1],uuid_chunk4[2],uuid_chunk4[3],
		      uuid_chunk4[4],uuid_chunk4[5],uuid_chunk4[6],uuid_chunk4[7]
	);
}

static struct kobj_attribute vmgenid_attribute = __ATTR(vmgenid, 0660, sysfs_show, NULL);

static int get_vmgenid(acpi_handle handle) {
	int i;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;
	union acpi_object *pss;
	int *virtual_addr;
	u64 uuid[2];
	u32 *uuid_chunk1;
	u16 *uuid_chunk2;
	u16 *uuid_chunk3;
	u8 *uuid_chunk4;
	
	printk(KERN_INFO "get vmgenid function\n");
	status = acpi_evaluate_object(handle, "ADDR", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		printk(KERN_INFO "evaluating object vmgenid address failed!\n");
	}

	pss = buffer.pointer;
	if (!pss ||
	    pss->type != ACPI_TYPE_PACKAGE) {
		 printk(KERN_INFO "vmgenid ADDR does not return package!\n");
		return -EFAULT;
	}		
	
	if (pss->package.count != 2) {
		printk(KERN_INFO "vmgenid ADDR package does not contain 2 fields\n");
		return -EFAULT;
	}

	printk(KERN_INFO "package count: %d\n", pss->package.count);	
	phy_addr = 0;
	for (i = 0; i < pss->package.count; i++) {

		union acpi_object *element = &(pss->package.elements[i]);
		if (element->type != ACPI_TYPE_INTEGER) {
			printk(KERN_INFO "element type is not integer!\n");
		}
		printk(KERN_INFO "value of entry %d is %lld\n", i, element->integer.value);
		phy_addr |= element->integer.value << i*32;
	}
	printk(KERN_INFO "value of phy_addr: %llu\n", phy_addr);	

	//Debug printing!
	virtual_addr = acpi_os_map_iomem(phy_addr,16); 

	printk(KERN_INFO "virtual address is: %n\n", virtual_addr);
	if (!virtual_addr) {
		printk(KERN_INFO "could not map vmgenid virtual memory!\n");
		return -EFAULT;
	}
	memcpy_fromio(uuid, virtual_addr, 16);

	uuid_chunk1 = (u32*)uuid;
	uuid_chunk2 = (u16*)(uuid_chunk1 + 1);
	uuid_chunk3 = (u16*)(uuid_chunk2 + 1);
	uuid_chunk4 = (u8*)(uuid_chunk3 + 1);
	printk(KERN_INFO "vmgenid value: %x-%x-%x-%x%x-%x%x%x%x%x%x\n", *uuid_chunk1, *uuid_chunk2, *uuid_chunk3,
	       uuid_chunk4[0],uuid_chunk4[1],uuid_chunk4[2],uuid_chunk4[3],uuid_chunk4[4],uuid_chunk4[5],uuid_chunk4[6],
               uuid_chunk4[7]);
	acpi_os_unmap_iomem(virtual_addr,16);
	return 0;	
}

static int acpi_vmgenid_add(struct acpi_device *device) {
	printk(KERN_INFO "vmgenid add\n");
	return get_vmgenid(device->handle);
}

static int acpi_vmgenid_remove(struct acpi_device *device) {
	printk(KERN_INFO "vmgenid remove\n");
	return 0;
}

static void acpi_vmgenid_notify(struct acpi_device *device, u32 event) {
	printk(KERN_INFO "vmgenid notify\n");
}

static const struct acpi_device_id vmgenid_ids[] = {
	{"QEMUVGID", 0},
	{"", 0},
};

static struct acpi_driver acpi_vmgenid_driver = {
	.name = "VGEN",
	.class = "VGIA",
	.ids = vmgenid_ids,
        .owner = THIS_MODULE,
	.ops = {
		.notify = acpi_vmgenid_notify,
		.add = acpi_vmgenid_add,
		.remove = acpi_vmgenid_remove,
	}
};

static int __init vmgenid_init(void) {
	int error = 0;
	printk(KERN_INFO "Loading vmgenid\n");
	//SYSFS entry
	vmgenid_kobj = kobject_create_and_add("idgar", kernel_kobj);
	
	if (!vmgenid_kobj)
		return -ENOMEM;

	error = sysfs_create_file(vmgenid_kobj, &vmgenid_attribute.attr);
	if (error) {
		printk(KERN_INFO "failed to create vmgenid sysfs file\n");
	}

	//Registration of acpi driver

	error = acpi_bus_register_driver(&acpi_vmgenid_driver);
	if (error < 0)
		return error;
	return 0;
}

static void __exit vmgenid_exit(void) {
	printk(KERN_INFO "exiting vmgenid\n");
	kobject_put(vmgenid_kobj);
        acpi_bus_unregister_driver(&acpi_vmgenid_driver);
}

module_init(vmgenid_init);
module_exit(vmgenid_exit);
