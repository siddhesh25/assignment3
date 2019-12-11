#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h> 
#include <linux/fs.h>      
#include <asm/atomic.h>
#include <asm/uaccess.h>   
#include <linux/unistd.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Siddhesh");

#define S2FS_VAR 0x19920342

#define TMPSIZE 20

/*
Inode is used to make a file or directory in the file system
*/

static struct inode *s2fs_make_inode(struct super_block *sb, int mode, const struct file_operations* s2fs_fops) //Part 3.1
{
        struct inode* inode;
        inode = new_inode(sb);
       if (!inode) {
                return NULL;
        }
        inode->i_mode = mode;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        inode->i_fop = s2fs_fops;
        inode->i_ino = get_next_ino(); //Part 3.1
        return inode;

}


/*
 * Open a file*/

static int s2fs_open(struct inode *inode, struct file *filp) //Part 3.3
{
        filp->private_data = inode->i_private;
        return 0;
}

/*
 * Read a file. Tried implementing the file to read hellp world
*/

static ssize_t s2fs_read_file(struct file *filp, char *buf,
                size_t count, loff_t *offset) //Part 3.3
{
        atomic_t *counter = (atomic_t *) filp->private_data;
        int v, len;
        char tmp[TMPSIZE];

        if (*offset > 0)
                v -= 1; 
        len = snprintf(tmp, TMPSIZE, "%d\n", v);
        if (*offset > len)
                return 0;
        if (count > len - *offset)
                count = len - *offset;

        if (copy_to_user(buf, tmp + *offset, count))
                return -EFAULT;
        *offset += count;
        return count;
}


/*
 * Write a file.
 */

static ssize_t s2fs_write_file(struct file *filp, const char *buf,
                size_t count, loff_t *offset) //Part 3.3
{
        atomic_t *counter = (atomic_t *) filp->private_data;
        char tmp[TMPSIZE];

 // Write at the start

        if (*offset != 0)
                return -EINVAL;


        if (count >= TMPSIZE)
                return -EINVAL;
        memset(tmp, 0, TMPSIZE);
        if (copy_from_user(tmp, buf, count))
                return -EFAULT;

        atomic_set(counter, simple_strtol(tmp, NULL, 10));
        return count;
}


/*
 File operations implementation
 */

static struct file_operations s2fs_file_ops = { //Part 3.3
        .open   = s2fs_open,
        .read   = s2fs_read_file,
        .write  = s2fs_write_file,
};


const struct inode_operations s2fs_inode_operations = { 
        .setattr        = simple_setattr,
        .getattr        = simple_getattr,
};

static struct dentry *s2fs_create_file (struct super_block *sb,
                struct dentry *dir, const char *dir_name,
                atomic_t *counter) //Part 3.4
{
        struct dentry *dentry;
        struct inode *inode;

        dentry = d_alloc_name(dir, dir_name);
        if (! dentry)
                goto out;
        inode = s2fs_make_inode(sb, S_IFREG | 0644, &s2fs_file_ops);
        if (! inode)
                goto out_dput;
        inode->i_private = counter;
        d_add(dentry, inode);
        return dentry;
  out_dput:
        dput(dentry);
  out:
        return 0;
}


/*
Creating dentry directory
 */
static struct dentry *s2fs_create_dir (struct super_block *sb,
                struct dentry *parent, const char *dir_name) //Part 3.2
{
        struct dentry *dentry;
        struct inode *inode;

        dentry = d_alloc_name(parent, dir_name);
        if (! dentry)
                goto out;

        inode = s2fs_make_inode(sb, S_IFDIR | 0755, &simple_dir_operations);
        if (! inode)
                goto out_dput;
        inode->i_op = &simple_dir_inode_operations;

        d_add(dentry, inode);
        return dentry;

  out_dput:
        dput(dentry);
  out:
        return 0;
}


static atomic_t counter, bar;

static void s2fs_create_files (struct super_block *sb, struct dentry *root) //Part 3.5
{
        struct dentry *foo;

        //atomic_set(&bar, 0);
	//perror("Hello World");
	//vfs_write(file, data, size, &offset);
        foo = s2fs_create_dir(sb, root, "foo");
        if (foo)
                s2fs_create_file(sb, foo, "bar", &bar);
}

static struct super_operations s2fs_s_ops = {
        .statfs         = simple_statfs,
        .drop_inode     = generic_delete_inode,
};

static int s2fs_fill_super (struct super_block *sb, void *data, int silent)
{
        struct inode *root;
        struct dentry *root_dentry;

        sb->s_blocksize = VMACACHE_SIZE;
        sb->s_blocksize_bits = VMACACHE_SIZE;
        sb->s_magic = S2FS_VAR;
        sb->s_op = &s2fs_s_ops;
        root = s2fs_make_inode (sb, S_IFDIR | 0755, &simple_dir_operations);
        inode_init_owner(root, NULL, S_IFDIR | 0755);
        if (! root)
                goto out;
        root->i_op = &simple_dir_inode_operations;
//      root->i_fop = &simple_dir_operations;
        set_nlink(root, 2);
        root_dentry = d_make_root(root);
        if (! root_dentry)
                goto out_iput;
        s2fs_create_files (sb, root_dentry);
        sb->s_root = root_dentry;
        return 0;

  out_iput:
        iput(root);
  out:
        return -ENOMEM;
}

static struct dentry *s2fs_get_super(struct file_system_type *fst,
                int flags, const char *devname, void *data)
{
        return mount_nodev(fst, flags, data, s2fs_fill_super);
}

static struct file_system_type s2fs_type = {
        .owner          = THIS_MODULE,
        .name       	= "s2fs",
        .mount          = s2fs_get_super,
        .kill_sb        = kill_litter_super,
};
static int __init s2fs_init(void)
{
        return register_filesystem(&s2fs_type);
}

static void __exit s2fs_exit(void)
{
        unregister_filesystem(&s2fs_type);
}

module_init(s2fs_init);
module_exit(s2fs_exit);
