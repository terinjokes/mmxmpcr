/**************************************************************************
   MMXMPCR - USB driver module for the XM Radio PCR

   (C) 2003 by Michael Minn - mmxmpcr@michaelminn.com

   Started - 6/7/03

   This program is free software; you can redistribute it and/or modify
   it under the terms of version 2 of the GNU General Public License as 
   published by the Free Software Foundation.

****************************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/PanedW.h>

#define MMXMPCR_VERSION "MMXMPCR v2003-06-14"
#define MMXMPCR_MAX_CHANNELS 171

struct mmxmpcr_context
{
	int device;
	XtAppContext application;
	Widget shell;
	Widget main_window;
	Widget list;
};


int dump_data(unsigned char *data, int length)
{
	if (length > 0)
	{
		for (int scan = 0; scan < length; ++scan)
			printf("%02x ", data[scan]);

		for (int scan = 0; scan < length; ++scan)
			if ((data[scan] > ' ') && (data[scan] <= 0x7f))
				printf("%c", data[scan]);
			else 
				printf("-");
	}

	printf("\n");

	return length;
}

int device_request(char *name, int device, int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7)
{
	// See USB 1.1 Spec pp 186 for Standard Device Request info
	//  ioctl() data is USB control request setup data
	//  Request Type (1 byte)
	//  Request Code (1 byte)
	//  Value (2 bytes)
	//  Index (2 bytes)
	//  Length (2 bytes)

	int status = 0;
	unsigned char data[256] = { d0, d1, d2, d3, d4, d5, d6, d7 };

	if ((status = ioctl(device, d6, data)) < 0)
	{
		status = -errno;
		fprintf(stderr, "ioctl() failure: %s\n", strerror(errno));
	}

	printf("%s (request %d) = %d: ", name, d1, status);
	dump_data(data, status);

	return status;
}

int device_read(char *name, int device, int length, unsigned char *data)
{
	int status = 0;
	memset(data, 0, length);

	if ((status = read(device, data, length)) < 0)
	{
		status = -errno;
		fprintf(stderr, "Failure reading: %s\n", strerror(errno));
	}

	// printf("%s: ", name);
	// dump_data(data, status);

	return status;
}

int wait_for_data(int device, int max_reads, int length, unsigned char *data)
{
	int status = 0;
	int received = 0;
	int timeout = 0;
	unsigned char buffer[256];

	for (timeout = max_reads; ((status = read(device, buffer, sizeof(buffer))) > 0) && timeout && ((received + status - 2) <= length); --timeout)
		if (status > 2)
		{
			memcpy(&data[received], &buffer[2], status - 2);
			received += (status - 2);
			if (received >= length)
				timeout = 1; // read limit
		}

	// printf("Read %d bytes\n", received);

	// dump_data(data, received);

	return received;
}

int device_write(char *name, int device, int length, int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7)
{
	int status = 0;
	unsigned char data[256] = { d0, d1, d2, d3, d4, d5, d6, d7 };

	if ((status = write(device, data, length)) < 0)
	{
		status = -errno;
		fprintf(stderr, "Failure writing %d bytes: %s\n", length, strerror(errno));
	}

	// printf("Wrote %s: ", name);
	// dump_data(data, length);

	return status;
}

int initialize(int device)
{
	int status = 0;
	for (int scan = 0; scan < 0x40; ++scan)
		status = device_request("Unknown", device, 0xc0, 0x90, 0x00, 0x00, (unsigned char) scan, 0x00, 0x02, 0x00);

	unsigned char buffer[256];

	/*************************************************************
	I have only a vague idea what this sequence of commands does
	It was copied from a USB Snoopy dump when the device was
	attached under Windoze. And it works, so I don't mess with it.
	**************************************************************/

	status = device_request("Get Descriptor", device, 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x0c, 0x00);
	status = device_request("Get Descriptor", device, 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x09, 0x00);
	status = device_request("Get Descriptor", device, 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x60, 0x00);
	status = device_request("Set Configuration", device, 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("Get Descriptor", device, 0x80, 0x06, 0x03, 0x03, 0x00, 0x00, 0x12, 0x00);
	status = device_request("Get Status?", device, 0xc0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00);
	status = device_request("URB 71", device, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 72", device, sizeof(buffer), buffer);
	status = device_request("URB 74", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 75", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 76-87", device, sizeof(buffer), buffer);
	status = device_request("URB 88", device, 0x40, 0x03, 0xc4, 0x09, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 90", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 91", device, 0x40, 0x04, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 92", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 93-105", device, sizeof(buffer), buffer);
	status = device_request("URB 106", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 107-118", device, sizeof(buffer), buffer);
	status = device_request("URB 119", device, 0x40, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 120-135", device, sizeof(buffer), buffer);
	status = device_request("URB 136", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 137", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 138-150", device, sizeof(buffer), buffer);
	status = device_request("URB 151", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 152", device, 0x40, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 153-167", device, sizeof(buffer), buffer);
	status = device_request("URB 168", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 169", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 171", device, 0xc0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00);
	status = device_request("URB 172", device, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 174", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 175", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 176-188", device, sizeof(buffer), buffer);
	status = device_request("URB 189", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 190", device, 0x40, 0x03, 0xc4, 0x09, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 191", device, 0x40, 0x04, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 192", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 193-205", device, sizeof(buffer), buffer);
	status = device_request("URB 206", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 207-218", device, sizeof(buffer), buffer);
	status = device_request("URB 219", device, 0x40, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 220-235", device, sizeof(buffer), buffer);
	status = device_request("URB 236", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 237", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 238-250", device, sizeof(buffer), buffer);
	status = device_request("URB 251", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 252", device, 0x40, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_read("URB 253-267", device, sizeof(buffer), buffer);
	status = device_request("URB 268", device, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 270", device, 0x40, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
	printf("Connect URB sequence complete\n");

	status = device_request("URB 271", device, 0xc0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00);
	status = device_request("URB 272", device, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 275", device, 0x40, 0x03, 0x38, 0x41, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 276", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 278", device, 0x40, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00);
	status = device_request("URB 279", device, 0x40, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 280", device, 0x40, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 281", device, 0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 283", device, 0x40, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00);
	status = device_request("URB 284", device, 0x40, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00);
	status = device_read("URB 285-287", device, sizeof(buffer), buffer);

	status = device_write("URB 288", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 289", device, 2, 0x00, 0x05, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 290", device, 5, 0x00, 0x10, 0x10, 0x10, 0x01, 0, 0, 0);
	status = device_write("URB 291", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 200, sizeof(buffer), buffer);

	status = device_write("URB 440", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 441", device, 2, 0x00, 0x02, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 442", device, 2, 0x70, 0x05, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 443", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 577", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 578", device, 2, 0x00, 0x01, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 579", device, 1, 0x31, 0, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 580", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 584", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 585", device, 2, 0x00, 0x02, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 586", device, 2, 0x42, 0x01, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 587", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 634", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 635", device, 2, 0x00, 0x02, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 636", device, 2, 0x13, 0x00, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 637", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 667", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 668", device, 2, 0x00, 0x01, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 669", device, 1, 0x43, 0, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 670", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 735", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 736", device, 2, 0x00, 0x01, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 737", device, 1, 0x43, 0, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 738", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	status = device_write("URB 743", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 745", device, 2, 0x00, 0x06, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 746", device, 6, 0x50, 0, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 747", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 40, sizeof(buffer), buffer);

	return status;
}

int change_channel(int device, int channel)
{
	unsigned char buffer[256];
	int status = device_write("URB 765", device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 767", device, 2, 0x00, 0x06, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 768", device, 6, 0x10, 0x01, channel, 0x00, 0x00, 0x01, 0, 0);
	status = device_write("URB 771", device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);
	status = wait_for_data(device, 50, sizeof(buffer), buffer);

	return status;
}

int refresh_channel(struct mmxmpcr_context *context, int channel)
{
	int status = device_write("URB 820", context->device, 2, 0x5a, 0xa5, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 821", context->device, 2, 0x00, 0x04, 0, 0, 0, 0, 0, 0);
	status = device_write("URB 822", context->device, 4, 0x25, 0x09, channel + 3, 0x00, 0, 0, 0, 0);
	status = device_write("URB 823", context->device, 2, 0xed, 0xed, 0, 0, 0, 0, 0, 0);

	int received = 0;
	int timeout = 50;
	unsigned char buffer[256];
	unsigned char raw_data[512];

	while (timeout && (received < 83) && (status >= 0))
		if ((status = read(context->device, buffer, sizeof(buffer))) < 0)
		{
			status = -errno;
			fprintf(stderr, "Failure reading channel information: %s\n", strerror(errno));
			return status;
		}

		else if (status <= 2)
			--timeout;

		else if ((received == 0) && (buffer[2] != 0x5a))
			--timeout;
	
		else 
		{
			memcpy(&raw_data[received], &buffer[2], status - 2);
			received += status - 2;
		}

	if (timeout <= 0)
	{
		fprintf(stderr, "Timed out waiting for channel %d data\n", channel);
		return -ETIMEDOUT;
	}
		
	// dump_data(buffer, status);

	char channel_data[256];
	sprintf(channel_data, "%03d: %16.16s %16.16s %16.16s %16.16s", raw_data[8], &raw_data[10], &raw_data[28], &raw_data[45], &raw_data[61]);
	XmString name = XmStringCreateSimple(channel_data);
	XmListReplaceItemsPos(context->list, &name, 1, raw_data[8] + 1);
	XmStringFree(name);

	return 0;
}

void refresh_list(XtPointer context_pointer, XtIntervalId *interval)
{
	struct mmxmpcr_context* context = (struct mmxmpcr_context*) context_pointer;

	static int current_channel = 0;
	refresh_channel(context, current_channel);
	current_channel = (current_channel + 1) % MMXMPCR_MAX_CHANNELS;

	XtAppAddTimeOut(context->application, 10, refresh_list, context_pointer);
}

void select_callback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct mmxmpcr_context *context = (struct mmxmpcr_context*) client_data;

	int selection_count = 0;
	int *selected_positions = NULL;
	XtVaGetValues(context->list,
		XmNselectedItemCount, &selection_count, 
		XmNselectedPositions, &selected_positions,
		NULL);

	if (selected_positions)
	{
		printf("Changing to channel %d\n", *selected_positions);	
		change_channel(context->device, (*selected_positions) - 1);
		refresh_channel(context, (*selected_positions) - 1);
	}
	else
		printf("No selections\n");
}

int main(int argc, char **argv)
{
	fprintf(stderr, "%s\n", MMXMPCR_VERSION);

	struct mmxmpcr_context context;
	context.device = open(MMXMPCR_DEVICE, O_RDWR);
	if (context.device < 0)
		return fprintf(stderr, "Failure opening XM PCR device %s: %s\n", MMXMPCR_DEVICE, strerror(errno));

	int status = initialize(context.device);
	if (status < 0)
		return fprintf(stderr, "Failure initializing XM PCR device %s: %s\n", argv[1], strerror(-status));

	String default_resources[] = {
		"mmxmpcr*background: white",
		"mmxmpcr*foreground: black",
		NULL };

	int fake_argc = 1;
	char *fake_argv = "mmxmpcr";
	context.shell = XtVaOpenApplication (&context.application, "mmxmpcr", NULL, 0,
		&fake_argc, &fake_argv, default_resources, sessionShellWidgetClass,
		XmNwidth, 600,
		XmNheight, 220,
		XmNtitle, MMXMPCR_VERSION,
		NULL);

	// Atom WM_DELETE_WINDOW = XInternAtom(XtDisplay(shell), "WM_DELETE_WINDOW", False);
	// XmAddWMProtocolCallback(shell, WM_DELETE_WINDOW, destroy_callback, (XtPointer) &context);

	context.main_window = XtVaCreateWidget("main_window", xmPanedWindowWidgetClass, context.shell,
		XmNsashWidth, 1,
		XmNsashHeight, 1, NULL);
	XtManageChild(context.main_window);

	context.list = XmCreateScrolledList(context.main_window, "list", NULL, 0);
	XtVaSetValues(context.list,
		XmNselectionPolicy, XmSINGLE_SELECT,
		XmNwidth, 600,
		XmNvisibleItemCount, 10,
		NULL);

	XtAddCallback(context.list, XmNdefaultActionCallback, select_callback, (XtPointer) &context);
	XtAddCallback(context.list, XmNsingleSelectionCallback, select_callback, (XtPointer) &context);
	XtManageChild(context.list);

	for (int init = 0; init <= MMXMPCR_MAX_CHANNELS; ++init)
	{
		char string[64];
		sprintf(string, "Channel %d", init);
		XmString name = XmStringCreateSimple(string);
		XmListAddItem(context.list, name, init + 1);
		XmStringFree(name);
	}

	XtAppAddTimeOut(context.application, 10, refresh_list, (XtPointer) &context);

	XtRealizeWidget(context.shell);

	XtAppMainLoop(context.application);

	return 0;
}

