#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/******************************************
 * 
 *          模块参数: module_param
 * 
 *******************************************/
static char *author_name = "DYWorker001";
module_param(author_name, charp, S_IRUGO);

static int author_age = 28;
module_param(author_age, int, S_IRUGO);


/******************************************
 * 
 *          导出符号: EXPORT_SYMBOL; EXPORT_SYMBOL_GPL
 * 
 *******************************************/
void entry_helloworld(void)
{
    printk("Welcome Hollworld\r\n");
}
EXPORT_SYMBOL(entry_helloworld);

 /***************************************
  * 
  *         模块初始化注册和声明
  * 
  ****************************************/
static int __init helloworld_init(void)
{
    printk("Hello World Module Init, author name = %s, author_age = %d\r\n", author_name, author_age);
    return 0;
}
module_init(helloworld_init);

static void __exit helloworld_exit(void)
{
    printk("Hello World Module Exit\r\n");
}
module_exit(helloworld_exit);

MODULE_AUTHOR("DYWorker001");
MODULE_DESCRIPTION("HelloWorld Module");
MODULE_LICENSE("GPL");

