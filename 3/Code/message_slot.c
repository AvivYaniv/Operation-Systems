#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>		/* for memory allocation */

#include "message_slot.h"

/* Module Description */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MessageSlot");
MODULE_AUTHOR("Aviv Yaniv");

/* Message Slot Dev Section */
#ifdef IS_CONCURRENT
struct chardev_info
{
  spinlock_t lock;
};
#endif

// used to prevent concurent access into the same device
#ifdef IS_CONCURRENT
static struct 	chardev_info device_info;
#endif

#ifdef IS_CONCURRENT
static int 		s_nDevOpenFlag = 0;
#endif

/* Prototype Functions for Message Slot character driver */
static int     	device_open(struct inode *, struct file *);
static ssize_t 	device_write(struct file*, const char __user*, size_t, loff_t*);
static ssize_t	device_read(struct file*, char __user*, size_t, loff_t*);
static long 	device_ioctl(struct file*, unsigned int, unsigned long);
static int     	device_release(struct inode *, struct file *);

static struct file_operations fopsDevice = {
	.owner 			= THIS_MODULE,
	.open 			= device_open,
	.write 			= device_write,
	.read 			= device_read,
	.unlocked_ioctl	= device_ioctl,
	.release 		= device_release,
};

/* Message List Section */
static LIST_HEAD(s_message_slot_list_head);
static LIST_HEAD(s_opened_devices_list_head);
static unsigned int s_uDevicesCounter							= 0;

/* Methods Section */
/*
 * Free message slots
 * 
 * Inspired by: https://isis.poly.edu/kulesh/stuff/src/klist/
 */
void freeMessagesSlotList(void)
{
	/* Struct Definition */
	struct message_slot_t*	pCurrentMessageSlotEntry;
	struct list_head* 		pos;
	struct list_head* 		temp;

	/* Code Section */
	/* Going over the messages slot list, 
	   freeing rows and pointers, 
	   in a safe manner as we modify it */
	list_for_each_safe(pos, temp, &s_message_slot_list_head){
		/* Fetching current message slot entry */
		pCurrentMessageSlotEntry = 
			list_entry(pos, message_slot_t, message_slot_list);

		/* Deleting the message slot node pointer */
		list_del(pos);

		/* Freeing message slot row entry */
		kfree(pCurrentMessageSlotEntry);
	}
}

/*
 * Free message devices
 * 
 * Inspired by: https://isis.poly.edu/kulesh/stuff/src/klist/
 */
void freeDevicesList(void)
{
	/* Struct Definition */
	struct opened_device_info_t*	pCurrentOpenedDeviceEntry;
	struct list_head* 				pos;
	struct list_head* 				temp;

	/* Code Section */
	/* Going over the messages devices list, 
	   freeing rows and pointers, 
	   in a safe manner as we modify it */
	list_for_each_safe(pos, temp, &s_opened_devices_list_head){
		/* Fetching current message device entry */
		pCurrentOpenedDeviceEntry = 
			list_entry(pos, opened_device_info_t, opened_devices_list);

		/* Deleting the message device node pointer */
		list_del(pos);

		/* Freeing message device row entry */
		kfree(pCurrentOpenedDeviceEntry);
	}
}

/*
 * Free message slots and devices
 */
void freeMessageLists(void)
{	
	/* Free message slots and devices */
	freeMessagesSlotList();
	freeDevicesList();

	/* Resetting counter */
	s_uDevicesCounter = 0;

	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_DATA_CLEARED);
	#endif
}

/* Device Read-Write Section */
/*
 * Going over opened devices list to determine if already opened
 */
BOOL isDeviceAlreadyOpened(const unsigned int device_minor_number)
{
	/* Struct Definition */
	struct opened_device_info_t* 	pCurrentDeviceRow;
	struct list_head* 				pos;
	
	/* Code Section */
	/* Going over the opened devices list, 
	   searching for matching row */
	list_for_each(pos, &s_opened_devices_list_head){
		/* Fetching current opened device entry */
		pCurrentDeviceRow = list_entry(pos, opened_device_info_t, opened_devices_list);

		/* If current device row matches */
		if (device_minor_number == pCurrentDeviceRow->device_minor_number)
		{
			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_INFO DEVICE_ALREADY_OPENED_FRMT, device_minor_number);
			#endif

			return TRUE;
		}
	}

	/* Device not found in devices list */
	return FALSE;
}

/*
 * Add new device to opened devices list
 * 
 * Inspired by: https://isis.poly.edu/kulesh/stuff/src/klist/	
 * 
 */
BOOL openNewDevice(const unsigned int device_minor_number)
{
	/* Struct Definition */
	struct opened_device_info_t* 			pDeviceRow;

	/* Code Section */
	/* Allocating device row */
	pDeviceRow = (opened_device_info_t*)kmalloc(sizeof(opened_device_info_t), GFP_KERNEL);

	/* Validating allocated successfully */
	if (!pDeviceRow)
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printk(KERN_ERR MEMORY_ALLOCATION_FAILED_MSG);
		#endif

		/* Returning */
		return FALSE;
	}

	/* Initializing new memory to zeroes */
	memset( pDeviceRow, 0, sizeof(opened_device_info_t) );

	/* Initializing opened device fields */		
	pDeviceRow->device_minor_number	= device_minor_number; 
	pDeviceRow->current_channel		= INVALID_CHANNEL_ID;

	/* Adding opened device to list */ 
	list_add(&(pDeviceRow->opened_devices_list), &s_opened_devices_list_head);

	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO ADDED_NEW_DEVICE_FRMT, device_minor_number);
	#endif

	/* Updating devices counter */
	++s_uDevicesCounter;

	/* Return success */
	return TRUE;
}

/*
 * Called when device is opened
 * 
 * Inspired by: https://www.tldp.org/LDP/lkmpg/2.4/html/c577.htm
 */
static int device_open( struct inode* inode,
                        struct file*  file )
{
	/* Variable Definition */
	#ifdef IS_CONCURRENT
	unsigned long 	flags; // for spinlock
	#endif

	BOOL			bResult;

	unsigned int 	device_minor_number = 0;

	#ifdef PRINT_DEBUG_MESSAGES
		static int 	sCounter = 0;
	#endif

	/* Code Section */
	#ifdef IS_CONCURRENT
	/* We don't want to talk to two processes at the same time */
	spin_lock_irqsave(&device_info.lock, flags);

	if( 1 == s_nDevOpenFlag )
	{		
		spin_unlock_irqrestore(&device_info.lock, flags);
		return -EBUSY;
	}

	++s_nDevOpenFlag;
	spin_unlock_irqrestore(&device_info.lock, flags);
	#endif
	
	/* Fetching device minor number */
	device_minor_number = iminor(inode);

	/* If device already opened */
	if (isDeviceAlreadyOpened(device_minor_number))
	{
		/* Nothing else to do */
		return SUCCESS;
	}

	/* Opening a new device */	
	if ((bResult = openNewDevice(device_minor_number)))
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_OPENED_D_TIMES_FRMT, sCounter++);
		#endif
	}
	
	/* Return */
	return bResult ? SUCCESS : -ENOMEM;
}

/*
 * Going over opened devices getting device channel
 */
unsigned int getDeviceChannel(const unsigned int device_minor_number)
{
	/* Struct Definition */
	struct opened_device_info_t* 	pCurrentDeviceRow;
	struct list_head* 				pos;
	
	/* Code Section */
	/* Going over the opened devices list, 
	   searching for matching row */
	list_for_each(pos, &s_opened_devices_list_head){
		/* Fetching current opened device entry */
		pCurrentDeviceRow = list_entry(pos, opened_device_info_t, opened_devices_list);

		/* If current device row matches */
		if (device_minor_number == pCurrentDeviceRow->device_minor_number)
		{			
			return pCurrentDeviceRow->current_channel;
		}
	}

	/* Return invalid channel */
	return INVALID_CHANNEL_ID;
}

/*
 * Called when a process, which already opened the dev file, attempts to write to it.
 * Call recives a message from the user space from the selected channel by ioctl
 * 
 */
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
	/* Struct Definition */
	struct message_slot_t*		pMessageSlotRow;
	struct list_head* 			pos;

	/* Vairiable Section */
	int  						nBytesWritten			= 0;
	unsigned int				device_minor_number		= 0;
	unsigned int        		current_channel			= INVALID_CHANNEL_ID;
	
	/* Code Section */
	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_READ_BEGIN);
	#endif

	/* If messge size is invalid */
	if ((MESSAGE_MAX_LENGTH < 	length) || 
		(0 					>= 	length))
	{
		/* Return invalid message size */
		return -EMSGSIZE;
	}

	/* Setting device minor number */
	device_minor_number = iminor(file_inode(file));

	/* Finding device's channel */
	current_channel = getDeviceChannel(device_minor_number);

	/* If channel not yet set */
	if (INVALID_CHANNEL_ID == current_channel)
	{
		return -EINVAL;
	}

	/* If empty buffer */
	if (NULL == buffer)
	{
		return -EINVAL;
	}

	/* Finding message to send */
	/* Going over the message slots list, 
	   searching for matching row */
	list_for_each(pos, &s_message_slot_list_head){
		/* Fetching current opened message entry */
		pMessageSlotRow = list_entry(pos, message_slot_t, message_slot_list);

		/* If current message row matches */
		if ((device_minor_number 	== 	pMessageSlotRow->device_minor_number) &&
		    (current_channel		== 	pMessageSlotRow->channel))
		{
			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_INFO WRITE_MESSAGE_SLOT_FRMT, device_minor_number, current_channel);
			#endif

			/* Initializing the bytes written counter */
			nBytesWritten = 0;

			/* Getting message from user */
			while (length > nBytesWritten)
			{
				/* Getting char from user to message buffer */
				if (0 > get_user(pMessageSlotRow->buffer[nBytesWritten], &buffer[nBytesWritten]))
				{
					return -EFAULT;
				}

				#ifdef PRINT_DEBUG_MESSAGES
					printk(KERN_INFO CHAR_FORMAT, buffer[nBytesWritten]);
				#endif

				/* Update the bytes write counter */
				++nBytesWritten;
			}

			/* If message not written */
			if (0 == nBytesWritten)
			{
				return -EMSGSIZE;
			}

			/* Setting message length, adjusting minus one because last advance in loop */
			pMessageSlotRow->length = nBytesWritten;

			/* Setting the null-termination to indicate end of message */
			pMessageSlotRow->buffer[nBytesWritten] = END_OF_MESSAGE;

			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_WRITE_FINISHED);
			#endif

			/* Return number of bytes written */
			return nBytesWritten;
		}
	}

	/* Invalid state, never supposed to get here */
	return -EINVAL;	
}

/*
 * Called when a process, which already opened the dev file, attempts to read from it.
 * Call sends a message to user space from the selected channel by ioctl
 * 
 * Inspired by: 
 * 		https://www.tldp.org/LDP/lkmpg/2.4/html/c577.htm
 * 		https://gist.github.com/ksvbka/6d6a02c6e8dddea2e0f2
 * 		http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/
 */
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
	/* Struct Definition */
	struct message_slot_t*		pMessageSlotRow;
	struct list_head* 			pos;

	/* Vairiable Section */
	int  						nBytesRead 				= 0;
	unsigned int				device_minor_number		= 0;
	unsigned int        		current_channel			= INVALID_CHANNEL_ID;

	/* Code Section */
	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_READ_BEGIN);
	#endif

	/* Setting device minor number */
	device_minor_number = iminor(file_inode(file));

	/* Finding device's channel */
	current_channel = getDeviceChannel(device_minor_number);

	/* If channel not yet set */
	if (INVALID_CHANNEL_ID == current_channel)
	{
		return -EINVAL;
	}

	/* If empty buffer */
	if (NULL == buffer)
	{
		return -ENOSPC;
	}

	/* Finding message to send */
	/* Going over the message slots list, 
	   searching for matching row */
	list_for_each(pos, &s_message_slot_list_head){
		/* Fetching current opened message entry */
		pMessageSlotRow = list_entry(pos, message_slot_t, message_slot_list);

		/* If current message row matches */
		if ((device_minor_number 	== 	pMessageSlotRow->device_minor_number) &&
		    (current_channel		== 	pMessageSlotRow->channel))
		{
			/* If message not yet set */
			if (0 == pMessageSlotRow->length)
			{
				return -EWOULDBLOCK;
			}

			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_INFO READ_MESSAGE_SLOT_FRMT, device_minor_number, current_channel);
			#endif

			/* User buffer is insufficient */
			if (length < pMessageSlotRow->length)
			{
				#ifdef PRINT_DEBUG_MESSAGES
					printk(KERN_ERR USER_BUFFER_IS_INSUFFICIENT);
				#endif

				return -ENOSPC;
			}

			/* Putting message to user */
			for (nBytesRead = 0; 
				 ((MESSAGE_MAX_LENGTH 	> 	nBytesRead) && 
				  (nBytesRead 			< 	pMessageSlotRow->length));
				 ++nBytesRead)
			{
				/* Putting char from message buffer to user */
				if (0 > put_user(pMessageSlotRow->buffer[nBytesRead], &buffer[nBytesRead]))
				{
					return - EFAULT;
				}

				#ifdef PRINT_DEBUG_MESSAGES
					printk(KERN_INFO CHAR_FORMAT, pMessageSlotRow->buffer[nBytesRead]);
				#endif
			}

			/* If message not read, never supposed to happen */
			if (0 == nBytesRead)
			{
				return -EWOULDBLOCK;
			}

			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_READ_FINISHED);
			#endif

			/* Return number of bytes read */
			return nBytesRead;
		}
	}

	/* Invalid state, never supposed to get here */
	return -EINVAL;	
}

/*
 * Going over message slot list to determine if already exist
 */
BOOL isMessageSlotExists(const unsigned int 	device_minor_number,
						 const unsigned int		channel)
{
	/* Struct Definition */
	struct message_slot_t* 		pCurrentMessageSlotRow;
	struct list_head* 			pos;
	
	/* Code Section */
	/* Going over the message slot list, 
	   searching for matching row */
	list_for_each(pos, &s_message_slot_list_head){
		/* Fetching current message slot entry */
		pCurrentMessageSlotRow = list_entry(pos, message_slot_t, message_slot_list);

		/* If current message row matches */
		if ((device_minor_number 	== 	pCurrentMessageSlotRow->device_minor_number) &&
		    (channel				== 	pCurrentMessageSlotRow->channel))
		{
			return TRUE;
		}
	}

	/* Device not found in devices list */
	return FALSE;
}

/*
 * Add new message slot to list
 * 
 * Inspired by: https://isis.poly.edu/kulesh/stuff/src/klist/	
 * 
 */
BOOL addNewMessageSlot(const unsigned int 	device_minor_number,
					   const unsigned int	channel)
{
	/* Struct Definition */
	struct message_slot_t* 			pMessageSlotRow;

	/* Code Section */
	/* Allocating message slot row */
	pMessageSlotRow = (message_slot_t*)kmalloc(sizeof(message_slot_t), GFP_KERNEL);

	/* Validating allocated successfully */
	if (!pMessageSlotRow)
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printk(KERN_ERR MEMORY_ALLOCATION_FAILED_MSG);
		#endif

		/* Returning failure */
		return FALSE;
	}

	/* Initializing new memory to zeroes */
	memset( pMessageSlotRow, 0, sizeof(message_slot_t) );

	/* Initializing memory slot fields */		
	pMessageSlotRow->device_minor_number	= device_minor_number; 
	pMessageSlotRow->channel				= channel;
	pMessageSlotRow->length					= 0;
	pMessageSlotRow->buffer[0]				= END_OF_MESSAGE; 

	/* Adding message slot to list */ 
	list_add(&(pMessageSlotRow->message_slot_list), &s_message_slot_list_head);

	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO ADDED_NEW_MESSAGE_SLOT_FRMT, device_minor_number, channel);
	#endif

	/* Return success */
	return TRUE;
}

/*
 * Sets non-zero channel id for subsequent read/write operations with device
 * Updating opened device current channel
 * If devices channel is not opened, creating it with an empty message
 * 
 * Returns:
 * 	-1		-	EINVAL, If the passed channel id is 0
 * 	-1		-	EINVAL, If the passed command is not MSG_SLOT_CHANNEL
 * 	-1		-	EINVAL, If device not opened or failed to add new message slot
 * 
 */
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
	/* Struct Definition */
	struct opened_device_info_t* 	pCurrentDeviceRow;
	struct list_head* 				pos;

	/* Variable Definition */
	BOOL							bIsDeviceOpened;
	unsigned int 					device_minor_number;

	/* Code Section */
	/* If not message slot command */
	if( MSG_SLOT_CHANNEL != ioctl_command_id )
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printk(KERN_ERR 
				   DEVICE_NAME_MESSAGES_SLOT 
				   DEVICE_IOCTL_COMMAND_IS_INVALID_ERROR, 
				   ioctl_command_id);
		#endif

		return -EINVAL;
	}
	/* Else, it is a message slot command */
	else
	{
		/* Validating channel id */
		if ((INVALID_CHANNEL_ID >= 	ioctl_param) || 
			(MAX_CHANNEL_ID		< 	ioctl_param))
		{
			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_ERR 
					DEVICE_NAME_MESSAGES_SLOT 
					DEVICE_IOCTL_PARAMETER_IS_INVALID_ERROR, 
					ioctl_param);
			#endif

			return -EINVAL;
		}

		/* Fetching device minor number */
		device_minor_number = iminor(file_inode(file));

		/* Setting device not founed as opened */
		bIsDeviceOpened = FALSE;

		/* Setting valid channel id */
		/* Going over the opened devices list, 
		searching for matching row */
		list_for_each(pos, &s_opened_devices_list_head){
			/* Fetching current opened device entry */
			pCurrentDeviceRow = list_entry(pos, opened_device_info_t, opened_devices_list);

			/* If current device row matches */
			if (device_minor_number == pCurrentDeviceRow->device_minor_number)
			{
				/* Setting device current channel */
				pCurrentDeviceRow->current_channel = ioctl_param;

				/* Setting device found */
				bIsDeviceOpened = TRUE;

				/* Device found  ending loop */
				break;
			}
		}

		/* If device not opened yet */
		if (!bIsDeviceOpened)
		{
			#ifdef PRINT_DEBUG_MESSAGES
				printk(KERN_ERR DEVICE_IS_NOT_OPENED_ERROR, device_minor_number);
			#endif			

			/* Return failure */
			return -EINVAL;
		}

		/* If message slot already exists */
		if (isMessageSlotExists(device_minor_number, ioctl_param))
		{
			/* Return success */
			return SUCCESS;
		}

		/* Adding new message slot */
		return addNewMessageSlot(device_minor_number, ioctl_param) ? SUCCESS : -EINVAL;
	}

	/* Never supposed to get here */
	return -EINVAL;
}

/*
 * Called when dev is released, when there is no process using it.
 * 
 * Inspired by: 
 * 		https://www.tldp.org/LDP/lkmpg/2.4/html/c577.htm
 * 		https://gist.github.com/ksvbka/6d6a02c6e8dddea2e0f2
 * 		http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/
 */
static int device_release( struct inode* inode,
                           struct file*  file)
{
	/* Variable Definition */
	#ifdef PRINT_DEBUG_MESSAGES
		static int sCounter 	= 0;
	#endif

	/* Code Section */	
	#ifdef IS_CONCURRENT
	unsigned long flags			= 0;

	// ready for our next caller
	spin_lock_irqsave(&device_info.lock, flags);
	--s_nDevOpenFlag;
	spin_unlock_irqrestore(&device_info.lock, flags);
	#endif

	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_RELEASED_D_TIMES_FRMT, sCounter++);
	#endif

	return SUCCESS;
}

/* Device Setup Section */
void initializeDeviceStaticVariables(void)
{
	/* Code Section */

	#ifdef IS_CONCURRENT
	s_nDevOpenFlag				= 0;

	// init dev struct
	memset( &device_info, 0, sizeof(struct chardev_info) );
	spin_lock_init( &device_info.lock );
	#endif

	s_uDevicesCounter		= 0;
}

// Initialize the module - Register the character device
static int __init module_init_function(void)
{
	/* Variable Definition */
	int nReturnValue = 0;

	/* Code Section */
	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_INIT_BEGIN);
	#endif

	/* Initialize device variables */
	initializeDeviceStaticVariables();
	
	/* Register char device */
	if (0 > 
		(nReturnValue = 
		 register_chrdev(MAJOR_NUM, 
						 DEVICE_NAME_MESSAGES_SLOT, 
						 &fopsDevice)))
	{
		#ifdef PRINT_BASIC
			printk(KERN_ERR DEVICE_NAME_MESSAGES_SLOT DEVICE_INIT_FAILED);
		#endif

		/* Return error */
		return nReturnValue;
	}

	#ifdef PRINT_BASIC
		printk(KERN_INFO MESSAGE_SLOT_REGISTERED_MAJOR_NUMBER_FRMT, MAJOR_NUM);
	#endif

	#ifdef PRINT_INSTRUCTIONS
		printk(KERN_INFO HOW_TO_USE_DRIVER_INSTRUCTIONS_FRMT, MAJOR_NUM, DEVICE_NAME_MESSAGES_SLOT, MAJOR_NUM);
	#endif
	
	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_INIT_ENDED);
	#endif

	/* Return success */
	return SUCCESS;
}

static void __exit module_exit_function(void)
{
	/* Code Section */
	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_EXIT_BEGIN);
	#endif

	/* Free message list */
	freeMessageLists();

	/* Unregistering char device */		
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME_MESSAGES_SLOT);

	/* Initialize dev variables */
	initializeDeviceStaticVariables();

	#ifdef PRINT_DEBUG_MESSAGES
		printk(KERN_INFO DEVICE_NAME_MESSAGES_SLOT DEVICE_EXIT_ENDED);
	#endif
}

/* Define Macros Section */
module_init(module_init_function);
module_exit(module_exit_function);
