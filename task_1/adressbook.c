#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/hashtable.h> 
#include <linux/types.h>
#include <linux/xxhash.h>
#include <linux/string.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vlad Kozlovskiy");
MODULE_DESCRIPTION("Adressbook module implemetation");
MODULE_VERSION("0.01");

#define MESSAGE_BUFFER_SIZE 1024
#define NAME_MAX_LEN 20
#define SURNAME_MAX_LEN 20
#define PHONE_MAX_LEN 15
#define EMAIL_MAX_LEN 40
#define SEED 42

static char* message_buffer;
static int major_number;


struct user_data {

    char name[NAME_MAX_LEN];
    char surname[SURNAME_MAX_LEN];
    
    char phone_number[PHONE_MAX_LEN];
    char email[EMAIL_MAX_LEN];

    int age;
};

struct h_user_data {

    char name[NAME_MAX_LEN];
    char surname[SURNAME_MAX_LEN];
    
    char phone_number[PHONE_MAX_LEN];
    char email[EMAIL_MAX_LEN];

    int age;

    struct hlist_node node;
};

DECLARE_HASHTABLE(tbl, 3);


static struct h_user_data *get_usr(const char *surname) {


    u32 hash_val = xxh32(surname, strlen(surname), SEED);
    struct h_user_data *curr;

    hash_for_each_possible(tbl, curr, node, hash_val) {
        if (!strcmp(curr->surname, surname)) {
            return curr;
        }
    }

    return NULL;
}



static long add_user(const char *new_name, const char *new_surname, int new_age,
                     const char *new_phone_number, const char *new_email) {

    struct h_user_data *new_user = kmalloc(sizeof(struct h_user_data), GFP_KERNEL);

    if (!new_user) {
        pr_err("[ADRESSBOOK] allocation fault\n");
        return -EFAULT;
    }

    strcpy(new_user->name, new_name);
    strcpy(new_user->surname, new_surname);
    
    strcpy(new_user->phone_number, new_phone_number);
    strcpy(new_user->email, new_email);

    new_user->age = new_age;
    
    u32 new_hash_val = xxh32(new_surname, strlen(new_surname), SEED);
	
    hash_add(tbl, &new_user->node, new_hash_val);
    pr_info("[ADRESSBOOK] Succesfully added user: %s %s\n", new_user->name, new_user->surname);

    return 0;
}


static void del_user(struct h_user_data *user) {
    pr_info("[ADRESSBOOK] Succesfully removed user: %s %s\n", user->name, user->surname);
    hash_del(&user->node);
    kfree(user);
}



static int adressbook_open(struct inode *inode, struct file *file) {
    pr_info("[ADRESSBOOK] Succesfully opened device \n");
    return 0;
}

static ssize_t adressbook_read(struct file *file, char __user *user_buffer, size_t length, loff_t *offset) {

   if (*offset >= MESSAGE_BUFFER_SIZE) {
        return 0;
    }

  
   size_t curr_len = length;

   if (curr_len > (size_t)(MESSAGE_BUFFER_SIZE - *offset)){
	curr_len = (size_t)(MESSAGE_BUFFER_SIZE - *offset);
   }
   

    if (copy_to_user(user_buffer, message_buffer + *offset, curr_len)) {
        return -EFAULT;
    }

    *offset += curr_len;
    return curr_len;
}

static ssize_t adressbook_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset) {
    char *input_buffer = kmalloc(count, GFP_KERNEL);

    if (!input_buffer) {
        pr_err("[ADRESSBOOK] allocation fault\n");
        return -EFAULT;
    }

    if (copy_from_user(input_buffer, user_buffer, count)) {
        kfree(input_buffer);
        return -EFAULT;
    }

    if (strncmp(input_buffer, "ADD", 3) == 0) {

	    char name[NAME_MAX_LEN], surname[SURNAME_MAX_LEN], phone_number[PHONE_MAX_LEN], email[EMAIL_MAX_LEN];
            int age;
            sscanf(input_buffer, "ADD %s %s %d %s %s", name, surname, &age, phone_number, email);
	    pr_info("[DEBUG] Scaned data %s\n", name); 
            add_user(name, surname, age, phone_number, email);
	   
    }

    else if (strncmp(input_buffer, "DELETE", 6) == 0) {

            char surname[SURNAME_MAX_LEN];
            sscanf(input_buffer, "DELETE %s", surname);
            struct h_user_data* curr_user = get_usr(surname);
            if (curr_user) {
                del_user(curr_user);
            } else {
                pr_info("[ADRESSBOOK] Cannot delete user %s because he is not in the table\n", surname);
            }

    }

    else if (strncmp(input_buffer, "FIND", 4) == 0) {
	
            char surname[SURNAME_MAX_LEN];
            sscanf(input_buffer, "FIND %s", surname);
            struct h_user_data* curr_user = get_usr(surname);

            if (curr_user) {
                memset(message_buffer, 0, MESSAGE_BUFFER_SIZE);
                snprintf(message_buffer, MESSAGE_BUFFER_SIZE,
                    "Name: %s \nSuranme: %s \nAge: %d\nPhone: %s\nEmail: %s\n\n",
                    curr_user->name, curr_user->surname, curr_user->age,
                    curr_user->phone_number, curr_user->email);
            } else {
                memset(message_buffer, 0, MESSAGE_BUFFER_SIZE);
                snprintf(message_buffer, MESSAGE_BUFFER_SIZE, "User not found\n");
            }
    }
    
    else{
            pr_info("[ADRESSBOOK] Invalid command");
    }

    kfree(input_buffer);
    return count;
}

static int adressbook_release(struct inode *inode, struct file *file) {
    pr_info("[ADRESSBOOK] Succesfully closed the device\n");
    return 0;
}


static const struct file_operations adressbook_fops = {
    .open = adressbook_open,
    .read = adressbook_read,
    .write = adressbook_write,
    .release = adressbook_release,
};




SYSCALL_DEFINE1(add_user, struct user_data*, intput_data)
{
    struct user_data new_user;
    
    if (copy_from_user(&new_user, (struct user_data*) intput_data, sizeof(struct user_data))) {
        pr_err("[ADRESSBOOK] Syscall add_user wasn't registered\n");
        return -EFAULT;
    }

    return add_user(new_user.name, new_user.surname, new_user.age, new_user.phone_number, new_user.email);
}


SYSCALL_DEFINE2(del_user, const char*, surname, unsigned int, len)
{
    char* k_surname = kmalloc(sizeof(char) * len, GFP_KERNEL);

    if (copy_from_user(k_surname, (const char*) surname, sizeof(char) * len)) {
        pr_err("[ADRESSBOOK] Syscall del_user wasn't registered\n");
        kfree(k_surname);
        return -EFAULT;
    }
    
    struct h_user_data* user = get_usr(k_surname);

    if (user){
    	del_user(user);
    }
    else{
	
	pr_err("[ADRESSBOOK] Can't remove user %s because he isn't in the adressbook\n", k_surname);
	kfree(k_surname);
    	return -1;

    }
    kfree(k_surname);
    return 0;
}

SYSCALL_DEFINE3(get_user, const char*, surname, unsigned int, len, struct user_data*, output_data)
{

    char* k_surname = kmalloc(sizeof(char) * len, GFP_KERNEL);
    struct user_data user;

    
    if (copy_from_user(k_surname, (const char*) surname, sizeof(char) * len)) {
        pr_err("[ADRESSBOOK] Syscall get_user wasn't registered\n");
        kfree(k_surname);
        return -EFAULT;
    }

    struct h_user_data* h_user = get_usr(k_surname);

    if (h_user) {
  
    strcpy(user.name, h_user->name);
    strcpy(user.surname, h_user->surname);

    strcpy(user.phone_number, h_user->phone_number);
    strcpy(user.email, h_user->email);

    user.age = h_user->age;

    if (copy_to_user((struct user_data *)output_data, &user, sizeof(struct user_data))) {
        	pr_err("[ADRESSBOOK] Syscall get_user wasn't registered\n");
        	kfree(k_surname);
        	return -EFAULT;
    	}

    return 1;
    }
    
    pr_info("[ADRESSBOOK] User not found\n");
    kfree(k_surname);
    return 0;
}



static int __init adressbook_init(void) {

    message_buffer = kmalloc(MESSAGE_BUFFER_SIZE, GFP_KERNEL);
    major_number = register_chrdev(0, "adressbook", &adressbook_fops);
    hash_init(tbl);

    if (major_number < 0) {
        pr_err("[ADRESSBOOK] Can't register majot number\n");
        return major_number;
    }
    
    pr_info("[ADRESSBOOK] Successfully loaded the module with major number %d\n", major_number);
    return 0;
}

static void __exit adressbook_exit(void) {

    kfree(message_buffer);
    struct h_user_data *curr;
    u32 tmp = 0;;

    hash_for_each(tbl, tmp, curr, node) {
        hash_del(&curr->node);
        kfree(curr->name);
	kfree(curr->surname);
	kfree(curr->phone_number);
	kfree(curr->email);
    }
	
    kfree(curr);
    unregister_chrdev(major_number, "adressbook");
    pr_info("[ADRESSBOOK] Succesfully unloaded module\n");
}

module_init(adressbook_init);
module_exit(adressbook_exit);
