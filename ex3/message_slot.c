#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   
#include <linux/module.h>   
#include <linux/fs.h>       
#include <linux/uaccess.h>  
#include <linux/string.h>  
#include <linux/slab.h>
#include "message_slot.h"


MODULE_LICENSE("GPL");



//================== DEFINE STRUCTS  ============================


struct info {
  int size;
  char message[MAX_MESSAGE_LEN];
};


struct slot_node{
    int minor;
    struct channel_node* channels_tree;
    struct channel_node* set_channel;
    struct slot_node* next;
};

struct channel_node { 
    int value;
    struct info* info;
    struct channel_node* left_child;
    struct channel_node* right_child;

};

struct data { 
  struct channel_node* set_channel;
  struct slot_node* device_slot;
};

//================== STRUCTS FUNCTIONS ===========================

static struct slot_node* message_slot_list[257];

struct slot_node* new_slot(int minor) {
    struct slot_node *temp;
    temp = (struct slot_node*)kmalloc(sizeof(struct slot_node),GFP_KERNEL);
    if (temp == NULL)
      return NULL;
    temp->minor = minor;
    temp->channels_tree = NULL;
    temp-> set_channel = NULL;
    temp->next = NULL;
    return temp;
}

// Search method
struct channel_node* search_channel (struct channel_node* node, int value) {
        if (node == NULL || node->value == value)
            return node;

        else if(value > node->value) 
            return search_channel(node->right_child, value);

        else 
            return search_channel(node-> left_child, value);
}

// new channel node

struct channel_node* new_channel(int value){
    struct channel_node* channel = kmalloc(sizeof(struct channel_node), GFP_KERNEL);
    if (channel == NULL)
      return NULL;
    
    channel->value = value;
    channel->info = NULL;
    channel->left_child = NULL;
    channel->right_child = NULL;
    return channel;
}

// insert new channel node to the tree 

struct channel_node* insert_channel(struct channel_node *root, struct channel_node* new_channel) {
        if(root==NULL)
            return new_channel;

        else if(new_channel->value > root->value) 
            root->right_child = insert_channel(root->right_child, new_channel);
        else 
            root->left_child = insert_channel(root->left_child,new_channel);
        return root;
}

struct info* new_info(void){
  struct info* info = kmalloc(sizeof(struct info), GFP_KERNEL);
  if (info == NULL)
    return NULL;
  
  return info;
  
}

//================== FREE MEMORY FUNCTIONS ===========================

static void free_channels (struct channel_node* device_channels_tree){
  struct channel_node* curr = device_channels_tree;

  if (curr == NULL)
    return;
  
  free_channels(curr->right_child);
  free_channels(curr->left_child);
  kfree(curr->info);
  kfree(curr);
  return;
}

//================== DEVICE FUNCTIONS ===========================

static int device_open( struct inode* inode, struct file*  file)
{
  unsigned int minor;
  struct data* file_data = kmalloc(sizeof(struct data), GFP_KERNEL);  
  if (file_data == NULL)
    return -EINVAL;
  
  minor = iminor(inode);

  if (message_slot_list[minor] == NULL){
      message_slot_list[minor] = new_slot(minor);
      if (message_slot_list[minor] == NULL)
        return -EINVAL;
  }
  
  file_data->device_slot = message_slot_list[minor];
  file_data->set_channel = NULL;
  file-> private_data = file_data;
  return SUCCESS;

}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{ 
  if (file->private_data != NULL)
    kfree(file->private_data);
  return SUCCESS;
}

//---------------------------------------------------------------
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
  int i;
  struct data* device_data = (struct data*)(file->private_data);

  if (device_data->set_channel == NULL || buffer == NULL)
    return -EINVAL;  
  
  if ((device_data->set_channel)-> info == NULL){
    return -EWOULDBLOCK;
  }
  
  if ((device_data->set_channel)->info->size > length){
    return -ENOSPC;
  }
  
  for (i = 0; i < (device_data->set_channel)->info->size; i++){
    put_user((device_data->set_channel)->info->message[i],&buffer[i]); // check id this is the direction
  }

  return i;
}

//---------------------------------------------------------------
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             len,
                             loff_t*            offset)
{
    int i;
    struct data* device_data = (struct data*)(file->private_data);

    if (device_data-> set_channel == NULL){
        return -EINVAL;
    }
    
    if (len == 0 || len > MAX_MESSAGE_LEN || buffer == NULL){
      return -EMSGSIZE;
    }
    
    if ((device_data->set_channel)->info == NULL){
      (device_data->set_channel)->info = new_info();
      if ((device_data->set_channel)->info == NULL)
        return -EINVAL;
    }
      
    for(i = 0; i < len; i++){
        get_user((device_data->set_channel)->info->message[i], &buffer[i]);
    }

   (device_data->set_channel)->info->size = i;

    return i;

}

static long device_ioctl (struct file* file, unsigned int cmd, unsigned long param) {

    struct data* device_data = (struct data*)(file->private_data);

    if (cmd != MSG_SLOT_CHANNEL || param == 0) {
        return -EINVAL;
    }

    
    device_data->set_channel = search_channel(device_data->device_slot->channels_tree, param);

    if (device_data->set_channel == NULL) {
      device_data-> set_channel = new_channel(param);
      if (device_data->set_channel == NULL){
        return -EINVAL;
      } 
      device_data->device_slot->channels_tree = insert_channel(device_data->device_slot->channels_tree, device_data->set_channel);
    }
    return SUCCESS;

}

//==================== DEVICE SETUP =============================


struct file_operations Fops = {
  .owner	  = THIS_MODULE,
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl   = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
static int simple_init(void)
{   
  int response;
  response = register_chrdev(MAJOR_NUMBER, DEVICE_RANGE_NAME, &Fops );

  if( response < 0 ) {
    printk(KERN_ALERT "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUMBER);
    return response;
  }

  printk("DRIVER INIT SUCCESFULLY\n");
  
  return SUCCESS;
}

//---------------------------------------------------------------
static void simple_cleanup(void)
{
  int i;
  for (i = 0; i < 257; i++){
    if (message_slot_list[i] != NULL){  
      free_channels(message_slot_list[i]->channels_tree);
      kfree(message_slot_list[i]);
    }
  }
  unregister_chrdev(MAJOR_NUMBER, DEVICE_RANGE_NAME);

}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
