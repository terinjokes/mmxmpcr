/**************************************************************************
   MMXMPCR - USB driver module for the XM Radio PCR

   (C) 2003 by Michael Minn - mmxmpcr@michaelminn.com

   See /usr/src/linux/Documentation/usb for more information about USB programming

   Remove /lib/modules/linux/drivers/usb/serial directory
   depmod then modifies /lib/modules/linux/modules.usbmap to remove ftdi_sio

   P:  Vendor=0403 ProdID=6001 Rev= 2.00
   S:  Manufacturer=FTDI
   S:  Product=USB <-> Serial
   S:  SerialNumber=12345678
   C:  #Ifs= 1 Cfg#= 1 Atr=80 MxPwr= 90mA
   I:  If#= 0 Alt= 0 #EPs= 2 Cls=ff(vend.) Sub=ff Prot=ff Driver=mmxmpcr
   E:  Ad=81(I) Atr=02(Bulk) MxPS=  64 Ivl=0ms
   E:  Ad=02(O) Atr=02(Bulk) MxPS=  64 Ivl=0ms
   T:  Bus=01 Lev=00 Prnt=00 Port=00 Cnt=00 Dev#=  1 Spd=12  MxCh= 2
   B:  Alloc=  0/900 us ( 0%), #Int=  0, #Iso=  0
   D:  Ver= 1.00 Cls=09(hub  ) Sub=00 Prot=00 MxPS= 8 #Cfgs=  1

   This program is free software; you can redistribute it and/or modify
   it under the terms of version 2 of the GNU General Public License as 
   published by the Free Software Foundation.

****************************************************************************/

#include <linux/modversions.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/init.h>


/************************* Version Info *****************************/

#define DRIVER_VERSION "v2003.06.14"
#define DRIVER_AUTHOR "Michael Minn"
#define DRIVER_DESC "Linux USB Module for the XM Radio PCR"


/****************** Hotplugging Identification Info ******************/

static struct usb_device_id mmxmpcr_ids [] =
{
	{ USB_DEVICE(0x403, 0x6001) },
	{ } /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, mmxmpcr_ids);


/************************* Globals *****************************/

#define MMXMPCR_PACKET_LENGTH 0x1000

static int mmxmpcr_in_endpoint = 0;
static int mmxmpcr_out_endpoint = 0;
static struct urb *mmxmpcr_urb = NULL;
static struct usb_device *mmxmpcr_device = NULL;
static wait_queue_head_t mmxmpcr_wait_queue;

static void mmxmpcr_wait_callback(struct urb *urb)
{
	if (waitqueue_active(&mmxmpcr_wait_queue))
		wake_up_interruptible(&mmxmpcr_wait_queue);
}

/************************* Linux Device File Operations *****************************/

static int mmxmpcr_file_open (struct inode *inode, struct file *file)
{
	if (!mmxmpcr_device)
		return -EHOSTDOWN;

	info("XM PCR opened");

	// MOD_INC_USE_COUNT;

	return 0;
}

static int mmxmpcr_file_release (struct inode *inode, struct file *file)
{
	if (!mmxmpcr_device)
		return -EHOSTDOWN;

	info("XM PCR closed");

	// MOD_DEC_USE_COUNT;

	return 0;
}

static ssize_t mmxmpcr_file_write (struct file *file, const char *user_data, size_t length, loff_t *offset)
{
	int status = 0;

	if ((!mmxmpcr_device) || (!mmxmpcr_urb))
		return -EHOSTDOWN;

	mmxmpcr_urb->status = 0;
	mmxmpcr_urb->dev = mmxmpcr_device;
	mmxmpcr_urb->transfer_flags = 0;
	mmxmpcr_urb->transfer_buffer_length = length;
	mmxmpcr_urb->complete = mmxmpcr_wait_callback;
	mmxmpcr_urb->pipe = usb_sndbulkpipe(mmxmpcr_device, mmxmpcr_out_endpoint);
	if (user_data && length)
		copy_from_user(mmxmpcr_urb->transfer_buffer, user_data, length);

	if ((status = usb_submit_urb(mmxmpcr_urb)) != 0)
	{
		err("failure %d submitting write urb", status);
		return status;
	}

	interruptible_sleep_on_timeout(&mmxmpcr_wait_queue, HZ * 5);

	if (mmxmpcr_urb->status == USB_ST_URB_PENDING)
	{
		err("Timed out waiting to send %d bytes", length);
		usb_unlink_urb(mmxmpcr_urb);
		return -ETIMEDOUT;
	}

	// info("Write %d bytes, URB status %d", length, mmxmpcr_urb->status);

	return length;
}

static ssize_t mmxmpcr_file_read (struct file *file, char *data, size_t length, loff_t *offset)
{
	int status = 0;

	/* info("read %d bytes", length); */

	if ((!mmxmpcr_device) || (!mmxmpcr_urb))
		return -EHOSTDOWN;

	mmxmpcr_urb->status = 0;
	mmxmpcr_urb->dev = mmxmpcr_device;
	mmxmpcr_urb->transfer_flags = 0;
	mmxmpcr_urb->transfer_buffer_length = MMXMPCR_PACKET_LENGTH;
	mmxmpcr_urb->complete = mmxmpcr_wait_callback;
	mmxmpcr_urb->pipe = usb_rcvbulkpipe(mmxmpcr_device, mmxmpcr_in_endpoint);

	if ((status = usb_submit_urb(mmxmpcr_urb)) != 0)
	{
		err("failure %d submitting read urb", status);
		return status;
	}

	interruptible_sleep_on_timeout(&mmxmpcr_wait_queue, 5 * HZ);

	if (mmxmpcr_urb->status != 0)
	{
		err("Failure %d on read", mmxmpcr_urb->status);
		usb_unlink_urb(mmxmpcr_urb);
		return -ETIMEDOUT;
	}

	// info("%d of %d bytes received", mmxmpcr_urb->actual_length, mmxmpcr_urb->transfer_buffer_length);

	copy_to_user(data, mmxmpcr_urb->transfer_buffer, mmxmpcr_urb->actual_length);

	return mmxmpcr_urb->actual_length;
}

static int mmxmpcr_file_ioctl (struct inode *inode, struct file *file, unsigned int command, unsigned long data)
{
	int status = 0;
	unsigned char request = 0;

	if ((!mmxmpcr_device) || (!mmxmpcr_urb))
		return -EHOSTDOWN;

	// USB Device Request - See USB 1.1 Spec pp 183
	// Data is pointer to "setup data" structure
	//    Request Type (1 byte)
	//    Request Code (1 byte)
	//    Value (2 bytes)
	//    Index (2 bytes)
	//    Length (2 bytes)
	
	mmxmpcr_urb->status = 0;
	mmxmpcr_urb->dev = mmxmpcr_device;
	mmxmpcr_urb->transfer_flags = 0;
	mmxmpcr_urb->transfer_buffer_length = command;
	mmxmpcr_urb->complete = mmxmpcr_wait_callback;
	if (command > 0)
		mmxmpcr_urb->pipe = usb_rcvctrlpipe(mmxmpcr_device, 0);
	else
		mmxmpcr_urb->pipe = usb_sndctrlpipe(mmxmpcr_device, 0);

	mmxmpcr_urb->setup_packet = (unsigned char*)kmalloc(8, GFP_KERNEL);
	memset(mmxmpcr_urb->setup_packet, 0, 8);
	copy_from_user(mmxmpcr_urb->setup_packet, (char*) data, 8);
	request = mmxmpcr_urb->setup_packet[1];

	if ((status = usb_submit_urb(mmxmpcr_urb)) != 0)
	{
		err("failure %d submitting write urb", status);
		return status;
	}

	interruptible_sleep_on_timeout(&mmxmpcr_wait_queue, HZ * 5);

	if (mmxmpcr_urb->status == USB_ST_URB_PENDING)
	{
		err("Timed out in ioctl()");
		usb_unlink_urb(mmxmpcr_urb);
		return -ETIMEDOUT;
	}

	copy_to_user((char*) data, mmxmpcr_urb->transfer_buffer, mmxmpcr_urb->actual_length);

	// info("Ioctl request %02x, %d of %d bytes returned, status %d", request, mmxmpcr_urb->actual_length, command, mmxmpcr_urb->status);

	if (mmxmpcr_urb->status == 0)
		return mmxmpcr_urb->actual_length;
	else
		return mmxmpcr_urb->status;
}


struct file_operations mmxmpcr_file_operations =
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	NULL, /* struct module *owner; */
#endif
	NULL, /* loff_t (*llseek) (struct file *, loff_t, int); */
	mmxmpcr_file_read, /* ssize_t (*read) (struct file *, char *, size_t, loff_t *); */
	mmxmpcr_file_write, /* ssize_t (*write) (struct file *, const char *, size_t, loff_t *); */
	NULL, /* int (*readdir) (struct file *, void *, filldir_t); */
	NULL, /* unsigned int (*poll) (struct file *, struct poll_table_struct *); */
	mmxmpcr_file_ioctl, /* int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long); */
	NULL, /* int (*mmap) (struct file *, struct vm_area_struct *); */
	mmxmpcr_file_open, /* int (*open) (struct inode *, struct file *); */
	NULL, /* int (*flush) (struct file *); */
	mmxmpcr_file_release, /* int (*release) (struct inode *, struct file *); */
	NULL, /* int (*fsync) (struct file *, struct dentry *); */
	NULL, /* int (*fasync) (int, struct file *, int); */
	NULL, /* int (*lock) (struct file *, int, struct file_lock *); */
	NULL, /* ssize_t (*readv) (struct file *, const struct iovec *, unsigned long, loff_t *); */
	NULL, /* ssize_t (*writev) (struct file *, const struct iovec *, unsigned long, loff_t *); */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	NULL, /* ssize_t (*writepage) (struct file *, struct page *, int, size_t, loff_t *, int); */
#endif
};


/************************* Debugging *****************************/

static void mmxmpcr_list_descriptors(struct usb_device *device)
{
	int configuration;
	for (configuration = 0; configuration < device->descriptor.bNumConfigurations; ++configuration)
	{
		int interface;
		info("Configuration %d, %d interfaces", configuration, device->config[configuration].bNumInterfaces);

		for (interface = 0; interface < device->config[configuration].bNumInterfaces; ++interface)
		{
			int alternate;
			info("Interface %d, %d alternates", interface, device->config[configuration].interface[interface].num_altsetting);
			for (alternate = 0; alternate < device->config[configuration].interface[interface].num_altsetting; ++alternate)
			{
				int endpoint;
				info("Alternate %d, %d endpoints", interface, device->config[configuration].interface[interface].altsetting[alternate].bNumEndpoints);
				for (endpoint = 0; endpoint < device->config[configuration].interface[interface].altsetting[alternate].bNumEndpoints; ++endpoint)
				{
					info("Endpoint %d: address %x, max packet %d, ",
						endpoint, device->config[configuration].interface[interface].altsetting[alternate].endpoint[endpoint].bEndpointAddress,
						device->config[configuration].interface[interface].altsetting[alternate].endpoint[endpoint].wMaxPacketSize);

					switch (device->config[configuration].interface[interface].altsetting[alternate].endpoint[endpoint].bmAttributes)
					{
						case USB_ENDPOINT_XFER_CONTROL: info("Control"); break;
						case USB_ENDPOINT_XFER_ISOC: info("Isosynchronous"); break;
						case USB_ENDPOINT_XFER_BULK: info("Bulk"); break;
						case USB_ENDPOINT_XFER_INT: info("Interrupt"); break;
					}
				}
			}
		}
	}
}

/************************* USB Module Registration, Entry, Exit *****************************/


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static void *mmxmpcr_probe(struct usb_device *device, unsigned interface, const struct usb_device_id *id)
#else
static void *mmxmpcr_probe(struct usb_device *device, unsigned interface)
#endif
{
	if ((device->descriptor.idVendor == 0x403) && (device->descriptor.idProduct == 0x6001) && (interface == 0))
	{
		info("XM PCR Connected");
		// mmxmpcr_list_descriptors(device);

		mmxmpcr_device = device;
		mmxmpcr_urb = usb_alloc_urb(0);
		mmxmpcr_urb->transfer_buffer = (unsigned char *)kmalloc(MMXMPCR_PACKET_LENGTH, GFP_KERNEL);

		mmxmpcr_in_endpoint = 0x81;
		mmxmpcr_out_endpoint = 0x02;

		return device;
	}

	return NULL;
}

static void mmxmpcr_disconnect(struct usb_device *device, void *driver_context)
{
	if (mmxmpcr_urb)
	{
		usb_unlink_urb(mmxmpcr_urb);
		kfree(mmxmpcr_urb->transfer_buffer);
		usb_free_urb(mmxmpcr_urb);
		mmxmpcr_urb = NULL;
	}

	mmxmpcr_device = NULL;

	info("XM PCR Disconnected");
}

static struct usb_driver mmxmpcr_driver_registration =
{
	name: "mmxmpcr",
	probe: mmxmpcr_probe,
	disconnect: mmxmpcr_disconnect,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	id_table:	NULL,
	driver_list: {NULL, NULL},
	fops: NULL
#endif
};

static int __init mmxmpcr_init(void)
{
	int status = 0;

	info(DRIVER_DESC " " DRIVER_VERSION);
	info("Major Number %d", MMXMPCR_MAJOR_NUMBER);

	init_waitqueue_head(&mmxmpcr_wait_queue);

	if ((status = register_chrdev(MMXMPCR_MAJOR_NUMBER, "mmxmpcr", &mmxmpcr_file_operations)) < 0)
	{
		err("register_chrdev() failed code %d", status);
		return -1;
	}

	if ((status = usb_register(&mmxmpcr_driver_registration)) < 0)
		err("Driver failed usb_register with code %d", status);

	return 0;
}

static void __exit mmxmpcr_cleanup(void)
{
	unregister_chrdev(MMXMPCR_MAJOR_NUMBER, "mmxmpcr");

	usb_deregister(&mmxmpcr_driver_registration);

	info("Module deregistered and unloaded");

	// return 0;
}

module_init(mmxmpcr_init);
module_exit(mmxmpcr_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");



