/**************************************************************************
   MMXMPCR - USB driver module for the XM Radio PCR

   (C) 2003 by Michael Minn - mmxmpcr@michaelminn.com

   Started - 6/7/03

   This program is free software; you can redistribute it and/or modify
   it under the terms of version 2 of the GNU General Public License as 
   published by the Free Software Foundation.

****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>
#include <signal.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Form.h>
#include <Xm/ScrollBar.h>
#include <X11/keysym.h> // includes keysymdef.h - needed for XK_* constants

#define MMXMPCR_VERSION "MMXMPCR v2004-01-11"
#define MMXMPCR_MAX_CHANNELS 199
#define MMXMPCR_CHANNELS_PER_PAGE 10

class mmxmpcr
{
	public:
	mmxmpcr();
	int send_request(int length, unsigned char *message);
	int get_response(unsigned char *message);
	int wait_for_response(unsigned char *message, int timeout_ms = 7000);
	int change_channel(int channel);
	int clear_list();
	int dump_message(unsigned char *message, int length);
	int handle_channel_info();
	
	int descriptor;
	int top_channel;
	int selected_channel;
	int next_channel; // next channel to get info for
	XtAppContext application;
	Widget shell;
	Widget main_window;
	Widget scrollbar;
	Widget list;

	int response_buffer_length;
	unsigned char response_buffer[512];
};

mmxmpcr::mmxmpcr()
{
	memset(this, 0, sizeof(*this));

	printf("Opening XM PCR on %s\n", MMXMPCR_DEVICE);

	descriptor = open(MMXMPCR_DEVICE, O_RDWR);
	if (descriptor < 0)
		exit(fprintf(stderr, "Failure opening XM PCR device %s: %s\n", MMXMPCR_DEVICE, strerror(errno)));

	struct termios terminal_info;
	memset(&terminal_info, 0, sizeof(terminal_info));
	if (tcgetattr(descriptor, &terminal_info) < 0)
		perror("tcgetattr failed");

	// unbuffered input so there's no wait for an EOL character
	// terminal_info.c_lflag &= (~ICANON);

	// Turn off all input processing
	memset(&terminal_info, 0, sizeof(terminal_info));

	if (tcsetattr(descriptor, TCSANOW, &terminal_info) < 0)
		perror("tcsetattr failed");

	unsigned char message[512];
	unsigned char power_on[11] = { 0x5A, 0xA5, 0x00, 0x05, 0x00, 0x10, 0x10, 0x10, 0x01, 0xED, 0xED };
	unsigned char get_radio_info[8] = { 0x5A, 0xA5, 0x00, 0x02, 0x70, 0x05, 0xED, 0xED };
	unsigned char get_radio_id[7] = { 0x5A, 0xA5, 0x00, 0x01, 0x31, 0xED, 0xED };
	unsigned char request42[8] = { 0x5A, 0xA5, 0x00, 0x02, 0x42, 0x01, 0xED, 0xED };
	unsigned char request13[8] = { 0x5A, 0xA5, 0x00, 0x02, 0x13, 0x00, 0xED, 0xED };
	unsigned char request50[12] = { 0x5A, 0xA5, 0x00, 0x06, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0xED, 0xED };
	unsigned char channel_info_request[10] = { 0x5A, 0xA5, 0x00, 0x04, 0x25, 0x09, 0x00, 0x00, 0xED, 0xED };

	if (send_request(11, power_on) < 0)
		exit(fprintf(stderr, "Failure sending power on message\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for power on response\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for power on acknowledgement\n"));

	else if (send_request(8, get_radio_info) < 0)
		exit(fprintf(stderr, "Failure getting radio info\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for radio info\n"));

	else if (send_request(7, get_radio_id) < 0)
		exit(fprintf(stderr, "Failure getting radio ID\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for radio ID\n"));

	else if (send_request(8, request42) < 0)
		exit(fprintf(stderr, "Failure getting radio init info\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for radio init ack 42\n"));

	else if (send_request(8, request13) < 0)
		exit(fprintf(stderr, "Failure getting radio init info\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for radio init ack 13\n"));

	else if (send_request(12, request50) < 0)
		exit(fprintf(stderr, "Failure getting radio init info\n"));

	else if (wait_for_response(message) < 0)
		exit(fprintf(stderr, "Failure waiting for radio init ack 50\n"));

	else if (send_request(10, channel_info_request) < 0)
		exit(fprintf(stderr, "Failure requesting initial channel info\n"));

	// unsigned char get_signal_strength[7] = { 0x5A, 0xA5, 0x00, 0x01, 0x43, 0xED, 0xED }; // sent twice
	// send_request(device, 7, get_signal_strength);
	// send_request(device, 7, get_signal_strength);

	printf("XM-PCR running\n");
}

int mmxmpcr::send_request(int length, unsigned char *message)
{
	// printf("Sending %d byte request\n", length);

	if (write(descriptor, message, length) < 0)
		exit(fprintf(stderr, "Failure writing request: %s\n", strerror(errno)));

	// printf("Sent\n");

	return length;
}

int mmxmpcr::get_response(unsigned char *message)
{
	//printf("Waiting for response\n");

	int status = 0; 
	struct pollfd poll_descriptor;
	poll_descriptor.fd = descriptor;
	poll_descriptor.events = POLLIN;
	poll_descriptor.revents = 0;

	if (poll(&poll_descriptor, 1, 10) < 0)
		perror("poll() failure");

	else if (poll_descriptor.revents && ((status = read(descriptor, &response_buffer[response_buffer_length], 96)) < 0))
		exit(fprintf(stderr, "Failure reading response: %s\n", strerror(errno)));

	else
		response_buffer_length += status;

	// dump_message(response_buffer, response_buffer_length);

	unsigned char *starter = (unsigned char*) memchr(response_buffer, 0x5a, response_buffer_length);
	if (!starter)
		response_buffer_length = 0;
	else if (starter != response_buffer)
	{
		response_buffer_length -= (starter - response_buffer);
		memmove(response_buffer, starter, response_buffer_length);
	}

	if (response_buffer_length <= 0)
		return 0;

	unsigned char *terminator = NULL;
	if (response_buffer_length > 2)
	{
		terminator = (unsigned char*) memchr(&response_buffer[2], 0x5a, response_buffer_length - 2);
		if (terminator && (*(terminator + 1) != 0xa5))
			terminator = NULL;
	}

	int message_length = 0;
	if (terminator)
		message_length = terminator - response_buffer;

	else if ((response_buffer_length >= 5) && (response_buffer[3] == 0xe3))
		message_length = 24;

	else if ((response_buffer_length >= 5) && (response_buffer[4] == 0x80)) // Hello
		message_length = 34;

	else if ((response_buffer_length >= 5) && (response_buffer[4] == 0x81)) // Goodbye
		message_length = 9;

	else if ((response_buffer_length >= 5) && (response_buffer[4] == 0x93)) // ??
		message_length = 9;

	else if ((response_buffer_length >= 5) && (response_buffer[4] == 0xE3)) // XM info
		message_length = 25;

	else if ((response_buffer_length >= 5) && (response_buffer[3] == 0xc2)) // ??
		message_length = 8;

	else if (response_buffer_length >= 4)
		message_length = response_buffer[3] + 6;

	if (message_length <= response_buffer_length)
	{
		memcpy(message, response_buffer, message_length);
		response_buffer_length -= message_length;
		memmove(response_buffer, &response_buffer[message_length], response_buffer_length);
		return message_length;
	}

	// printf("%d byte response received\n", length);
	// dump_message(message, length);

	return 0;
}

int mmxmpcr::wait_for_response(unsigned char *message, int timeout_ms)
{
	int length = 0;
	while ((timeout_ms >= 0) && ((length = get_response(message)) == 0))
	{
		usleep(100000);
		timeout_ms -= 100;
	}

	if (length == 0)
		return -ETIMEDOUT;

	return length;
}

int mmxmpcr::change_channel(int channel)
{
	// printf("Change channel %d (0x%x)\n", channel, channel);

	unsigned char message[512];
	
	//unsigned char change_channel_1[12]={ 0x5A, 0xA5, 0x00, 0x06, 0x50, selected_channel, 0x00, 0x00, 0x00, 0x00, 0xED, 0xED };
	// send_request(12, change_channel_1);
	// wait_for_response(message, 2000);

	unsigned char change_channel_3[12] = { 0x5A, 0xA5, 0x00, 0x06, 0x10, 0x02, channel, 0x00, 0x00, 0x01, 0xED, 0xED };
	send_request(12, change_channel_3);
	wait_for_response(message, 2000);

	//unsigned char change_channel_2[12] = { 0x5A, 0xA5, 0x00, 0x06, 0x50, channel, 0x01, 0x01, 0x01, 0x01, 0xED, 0xED };
	//send_request(12, change_channel_2);
	//wait_for_response(message, 2000);

	unsigned char get_channel_info[10] = { 0x5A, 0xA5, 0x00, 0x04, 0x25, 0x08, channel, 0x00, 0xED, 0xED };
	send_request(10, get_channel_info);

	selected_channel = channel;
	
	return channel;
}

int mmxmpcr::clear_list()
{
	XmListDeleteAllItems(list);
	
	for (int init = 0; init < MMXMPCR_CHANNELS_PER_PAGE; ++init)
	{
		char string[16];
		sprintf(string, "%02d:", top_channel + init);
		XmString name = XmStringCreateSimple(string);
		XmListAddItem(list, name, 0);
		XmStringFree(name);
	}
	
	return 0;
}


int mmxmpcr::handle_channel_info()
{
	// Issue 5AA5000425090000EDED, and it will return a channel. Then you ack 
	// with  5AA500042509xx00EDED for whatever channel it just sent.

	unsigned char raw_data[512];
	memset(raw_data, 0, sizeof(raw_data));

	int status = get_response(raw_data);
	if ((status < 9) || (raw_data[0] != 0x5a) || (raw_data[4] != 0xA5))
	{
		// dump_message(raw_data, status);
		return -1;
	}

	int list_channel = raw_data[7] - top_channel + 1;
	char channel_data[256];
	sprintf(channel_data, "%03d: %16.16s %16.16s %16.16s %16.16s", 
			raw_data[7], &raw_data[10], &raw_data[28], &raw_data[45], &raw_data[61]);

	// This is the closest I could figure out for a pattern relating raw_data[7] to raw_data[8]
	// raw_data[7] is always the channel number, but sometimes raw_data[8] is not the same as
	// raw_data[7], even if the channel exists.  If the channel doesn't exist, they are always
	// different, although exactly what this means is uncertain, other than if the requested
	// non-existant channel is less than 79, raw_data[7] will be greater than raw_data[8] and
	// if the non-existant channel is greater than 79, raw_data[7] will be larger.
	// Channel 171 seems to always be the top channel, except for the Playboy subscription channel.

	if (((raw_data[7] < 79) && (raw_data[7] >= raw_data[8])) ||
	    ((raw_data[7] > 79) && (raw_data[7] < 171) && (raw_data[7] <= raw_data[8])) ||
	    (raw_data[7] == 171) ||
	    ((raw_data[7] > 171) && (raw_data[7] == raw_data[8])))
	{
		XmString name = XmStringCreateSimple(channel_data);
		XmListReplaceItemsPos(list, &name, 1, list_channel);
		XmStringFree(name);

		if (selected_channel == raw_data[7])
			XmListSelectPos(list, list_channel, 0);
	}

	// printf("%03d %03d %03d: %s\n", raw_data[7], raw_data[8], raw_data[9], channel_data);

	++next_channel;

	if ((next_channel < top_channel) || (next_channel > (top_channel + MMXMPCR_CHANNELS_PER_PAGE)))
		next_channel = top_channel;

	unsigned char get_channel_info[10] = { 0x5A, 0xA5, 0x00, 0x04, 0x25, 0x08, next_channel, 0x00, 0xED, 0xED };
	send_request(10, get_channel_info);

	// printf("Requesting %d\n", next_channel);
	// dump_message(raw_data, status);

	return 0;
}

int mmxmpcr::dump_message(unsigned char *message, int length)
{
	for (int scan = 0; scan < length; ++scan)
	{
		printf("%02x ", message[scan]);

		if ((scan & 0xf) == 0xf)
			printf("\n");
	}

	printf("\n");

	for (int scan = 0; scan < length; ++scan)
	{
		if ((message[scan] < 32) || (message[scan] > 127))
			printf("-");
		else
			printf("%c", message[scan]);

		if ((scan & 0xf) == 0xf)
			printf("\n");
	}

	printf("\n\n");
	return length;
}

void refresh_list(XtPointer context_pointer, XtIntervalId *interval)
{
	mmxmpcr *context = (mmxmpcr*) context_pointer;

	context->handle_channel_info();

	XtAppAddTimeOut(context->application, 100, refresh_list, context_pointer);
}

void select_callback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	mmxmpcr *context = (mmxmpcr*) client_data;

	int selection_count = 0;
	int *selected_positions = NULL;
	XmString *strings = NULL;

	XtVaGetValues(context->list,
		XmNselectedItemCount, &selection_count, 
		XmNselectedPositions, &selected_positions,
		XmNselectedItems, &strings,
		NULL);

	char *text = NULL;
	
	if (!strings)
		printf("No selection strings\n");

	else if (XmStringGetLtoR(strings[0], XmFONTLIST_DEFAULT_TAG, &text) && text && text[0])
	{
		int channel = atoi(text);
		printf("Changing to channel %d\n", channel);	
		context->change_channel(channel);
		XtFree(text);
	}
	else
		printf("Bad selection string\n");
}

void scrollbar_callback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	mmxmpcr *context = (mmxmpcr*) client_data;
	
	int page = 0;
	XtVaGetValues(context->scrollbar, XmNvalue, &page, NULL);
	
	context->top_channel = page * MMXMPCR_CHANNELS_PER_PAGE;
	context->clear_list();
}

void key_press(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_flag)
{
	mmxmpcr *context = (mmxmpcr*) client_data;

	// Mouse wheel sends two messages for each notch of the wheel - ignore the second
	static int mouse = 0;
	++mouse;
	if ((mouse & 1) && (event->type & ButtonPressMask) && ((event->xbutton.button == Button4) || (event->xbutton.button == Button5)))
		return;
				
        // KeySym definitions are in /usr/X11R6/include/X11/keysymdef.h
	int keysym = XKeycodeToKeysym(event->xkey.display, event->xkey.keycode, 0);
	// printf("Event %x, sym %x, Code %x, button %x\n", event->type, keysym, event->xkey.keycode, event->xbutton.button);

	if ((event->type & ButtonPressMask) && (event->xbutton.button == Button4) && (context->top_channel > 0))
	{
		context->top_channel -= MMXMPCR_CHANNELS_PER_PAGE;
		context->clear_list();
		XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
	}

	else if ((event->type & KeyReleaseMask) && (keysym == XK_Prior) && (context->top_channel > 0))
	{
		context->top_channel -= MMXMPCR_CHANNELS_PER_PAGE;
		context->clear_list();
		XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
	}
	
	else if ((event->type & ButtonPressMask) && (event->xbutton.button == Button5) && (context->top_channel < MMXMPCR_MAX_CHANNELS))
	{
		context->top_channel += MMXMPCR_CHANNELS_PER_PAGE;
		context->clear_list();
		XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
	}
	
	else if ((event->type & KeyReleaseMask) && (keysym == XK_Next) && (context->top_channel < MMXMPCR_MAX_CHANNELS))
	{
		context->top_channel += MMXMPCR_CHANNELS_PER_PAGE;
		context->clear_list();
		XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
	}
	
	else if (((event->type & KeyReleaseMask) || (event->type & KeyPressMask)) && (keysym >= XK_0) && (keysym <= XK_9))
	{
		int entry = keysym - XK_0 + 1;
		// Select Position = 0 selects the last item in the list - MOTIF is madness :+(
		XmListSelectPos(context->list, entry, 1);
	}

	else if (((event->type & KeyReleaseMask) || (event->type & KeyPressMask)) && (keysym == XK_Up) && (context->selected_channel > 1))
	{
		context->change_channel(context->selected_channel - 1);
		if (context->selected_channel < (context->top_channel))
		{
			context->top_channel -= MMXMPCR_CHANNELS_PER_PAGE;
			context->clear_list();
			XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
		}
	}
	
	else if (((event->type & KeyReleaseMask) || (event->type & KeyPressMask)) && (keysym == XK_Down))
	{
		context->change_channel(context->selected_channel + 1);
		if (context->selected_channel >= (context->top_channel + MMXMPCR_CHANNELS_PER_PAGE))
		{
			context->top_channel += MMXMPCR_CHANNELS_PER_PAGE;
			context->clear_list();
			XtVaSetValues(context->scrollbar, XmNvalue, context->top_channel / MMXMPCR_CHANNELS_PER_PAGE, NULL);
		}
	}

	else if (((event->type & KeyReleaseMask) || (event->type & KeyPressMask)) && ((keysym == XK_Q) || (keysym == XK_q)))
	{
		printf("Exiting\n");
		exit(0);
	}

}

int main(int argc, char **argv)
{
	fprintf(stderr, "%s\n", MMXMPCR_VERSION);

	int process = fork();
	if (process < 0)
		return fprintf(stderr, "Failure forking: %s\n", strerror(errno));
	else if (process != 0)
		return process;

	mmxmpcr context;

	String default_resources[] = {
		"mmxmpcr*background: white",
		"mmxmpcr*foreground: black",
		NULL };

	int fake_argc = 1;
	char *fake_argv = "mmxmpcr";
	context.shell = XtVaOpenApplication (&context.application, "mmxmpcr", NULL, 0,
		&fake_argc, &fake_argv, default_resources, sessionShellWidgetClass,
		XmNwidth, 500,
		// XmNheight, 220,
		XmNtitle, MMXMPCR_VERSION,
		NULL);

	context.main_window = XtVaCreateManagedWidget("main_window", xmFormWidgetClass, context.shell, NULL);

	context.scrollbar = XtVaCreateManagedWidget("scrollbar", xmScrollBarWidgetClass, context.main_window,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,			
		XmNminimum, 0,
		XmNmaximum, (MMXMPCR_MAX_CHANNELS + 1) / MMXMPCR_CHANNELS_PER_PAGE,
		XmNvalue, 0,
		XmNpageIncrement, 1,
		XmNorientation, XmVERTICAL,
		NULL);
	XtAddCallback(context.scrollbar, XmNvalueChangedCallback, scrollbar_callback, (XtPointer) &context);
	
	context.list = XtVaCreateManagedWidget("list", xmListWidgetClass, context.main_window,
		XmNselectionPolicy, XmSINGLE_SELECT,
		XmNvisibleItemCount, MMXMPCR_CHANNELS_PER_PAGE,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, context.scrollbar,
		XmNbottomAttachment, XmATTACH_FORM,			
		NULL);
	context.clear_list();
	
	// The "0" key is translated to go to the end of the list - MMXMPCR needs it to go to the top
	// So, translations have to be removed and mouse event translations added - X is a mess
	XtUninstallTranslations(context.list);
	char *new_translations = 
		"<Btn1Down>: ListBeginSelect()\n"
		"<Btn1Up>: ListEndSelect()\n";
	 XtTranslations translations = XtParseTranslationTable(new_translations);
	 XtAugmentTranslations(context.list, translations);
	
	XtAddEventHandler(context.list, KeyPressMask, 0, (XtEventHandler) &key_press, (XtPointer) &context);
	XtAddEventHandler(context.list, ButtonPressMask, 0, (XtEventHandler) &key_press, (XtPointer) &context);
	XtAddCallback(context.list, XmNdefaultActionCallback, select_callback, (XtPointer) &context);
	XtAddCallback(context.list, XmNsingleSelectionCallback, select_callback, (XtPointer) &context);

	XtAppAddTimeOut(context.application, 10, refresh_list, (XtPointer) &context);

	XtRealizeWidget(context.shell);

	XtAppMainLoop(context.application);

	return 0;
}

