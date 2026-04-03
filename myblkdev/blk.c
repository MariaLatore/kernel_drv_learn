#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

#define NUM_SECTORS 1024
#define MY_SECTOR_SIZE 512
#define MY_BLOCK_SIZE 512

static int block_major = 0;
static unsigned char *block_memory;

struct my_block_dev {
  struct gendisk *disk;
  struct blk_mq_tag_set tag_set;
};

static struct my_block_dev *dev = NULL;

// The actual data transfer function
static blk_status_t block_queue_rq(struct blk_mq_hw_ctx *hctx,
                                   const struct blk_mq_queue_data *bd) {
  struct request *req = bd->rq;
  struct bio_vec bvec;
  struct req_iterator iter;
  unsigned long offset = blk_rq_pos(req) * MY_SECTOR_SIZE; // sector to bytes

  blk_mq_start_request(req);

  rq_for_each_segment(bvec, req, iter) {
    size_t len = bvec.bv_len;
    void *buffer = page_address(bvec.bv_page) + bvec.bv_offset;

    if ((offset + len) > (NUM_SECTORS * MY_BLOCK_SIZE)) {
      blk_mq_end_request(req, BLK_STS_IOERR);
      return BLK_STS_OK;
    }

    if (rq_data_dir(req) == WRITE)
      memcpy(block_memory + offset, buffer, len);
    else
      memcpy(buffer, block_memory + offset, len);
    offset += len;
  }
  blk_mq_end_request(req, BLK_STS_OK);
  return BLK_STS_OK;
}

static const struct blk_mq_ops block_mq_ops = {
    .queue_rq = block_queue_rq,
};

static const struct block_device_operations block_fops = {
    .owner = THIS_MODULE,
};

static int __init block_init(void) {
  pr_err("start init block");
  int rc;
  struct queue_limits lim = {
      .logical_block_size = 512,
      .physical_block_size = 512,
      .max_hw_sectors = 1024,
      .max_segments = 128,
      .max_segment_size = PAGE_SIZE,
      .io_min = MY_SECTOR_SIZE,
      .io_opt = MY_SECTOR_SIZE,
  };
  // 1.Allocate memory for storage
  block_memory = kzalloc(NUM_SECTORS * MY_BLOCK_SIZE, GFP_KERNEL);
  if (!block_memory) {
    pr_err("block_memory alloc failed");
    return -ENOMEM;
  }

  dev = kzalloc(sizeof(struct my_block_dev), GFP_KERNEL);
  if (!dev) {
    pr_err("dev alloc failed");
    kfree(block_memory);
    return -ENOMEM;
  }

  // 2.Register block device
  block_major = register_blkdev(0, "myblock");
  if (block_major < 0) {
    pr_err("register blkdev failed");
    goto err_reg;
  }

  // 3.Setup Multi-Queue Tag Set
  dev->tag_set.ops = &block_mq_ops;
  dev->tag_set.nr_hw_queues = 1;
  dev->tag_set.queue_depth = 128;
  dev->tag_set.numa_node = NUMA_NO_NODE;
  dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
  if (blk_mq_alloc_tag_set(&dev->tag_set)) {
    pr_err("alloc tag set failed");
    goto err_tag;
  }

  // 4.Allocate Disk (Modern API)
  dev->disk = blk_mq_alloc_disk(&dev->tag_set, &lim, NULL);
  if (IS_ERR(dev->disk)) {
    pr_err("alloc disk failed");
    goto err_disk;
  }

  dev->disk->major = block_major;
  dev->disk->first_minor = 0;
  dev->disk->fops = &block_fops;
  dev->disk->private_data = dev;
  dev->disk->minors = 1;
  snprintf(dev->disk->disk_name, 32, "zxyramdisk0");
  set_capacity(dev->disk, NUM_SECTORS);

  // 5.Add Disk
  rc = add_disk(dev->disk);
  if (rc) {
    pr_err("add disk failed:%d", rc);
    goto err_add;
  }

  return 0;

err_add:
  put_disk(dev->disk);
err_disk:
  blk_mq_free_tag_set(&dev->tag_set);
err_tag:
  unregister_blkdev(block_major, "myblock");
err_reg:
  kfree(dev);
  kfree(block_memory);
  return -ENOMEM;
}

static void __exit block_exit(void) {
  if (dev->disk) {
    del_gendisk(dev->disk);
    put_disk(dev->disk);
  }
  blk_mq_free_tag_set(&dev->tag_set);
  unregister_blkdev(block_major, "myblock");
  kfree(dev);
  kfree(block_memory);
}

module_init(block_init);
module_exit(block_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bpb");
