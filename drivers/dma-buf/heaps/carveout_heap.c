// SPDX-License-Identifier: GPL-2.0

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/genalloc.h>
#include <linux/of_reserved_mem.h>

struct carveout_heap_priv {
	struct dma_heap *heap;
	struct gen_pool *pool;
};

struct carveout_heap_buffer_priv {
	struct mutex lock;
	struct list_head attachments;

	unsigned long len;
	unsigned long num_pages;
	struct carveout_heap_priv *heap;
	void *buffer;
};

struct carveout_heap_attachment {
	struct list_head head;
	struct sg_table table;

	struct device *dev;
	bool mapped;
};

static int carveout_heap_attach(struct dma_buf *buf,
				struct dma_buf_attachment *attachment)
{
	struct carveout_heap_buffer_priv *priv = buf->priv;
	struct carveout_heap_attachment *a;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;
	INIT_LIST_HEAD(&a->head);
	a->dev = attachment->dev;
	attachment->priv = a;

	ret = sg_alloc_table(&a->table, priv->num_pages, GFP_KERNEL);
	if (ret)
		goto err_cleanup_attach;

	mutex_lock(&priv->lock);
	list_add(&a->head, &priv->attachments);
	mutex_unlock(&priv->lock);

	return 0;

err_cleanup_attach:
	kfree(a);
	return ret;
}

static void carveout_heap_detach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *attachment)
{
	struct carveout_heap_buffer_priv *priv = dmabuf->priv;
	struct carveout_heap_attachment *a = attachment->priv;

	mutex_lock(&priv->lock);
	list_del(&a->head);
	mutex_unlock(&priv->lock);

	sg_free_table(&a->table);
	kfree(a);
}

static struct sg_table *
carveout_heap_map_dma_buf(struct dma_buf_attachment *attachment,
			  enum dma_data_direction direction)
{
	struct carveout_heap_attachment *a = attachment->priv;
	struct sg_table *table = &a->table;
	int ret;

	ret = dma_map_sgtable(a->dev, table, direction, 0);
	if (ret)
		return ERR_PTR(-ENOMEM);

	a->mapped = true;

	return table;
}

static void carveout_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
					struct sg_table *table,
					enum dma_data_direction direction)
{
	struct carveout_heap_attachment *a = attachment->priv;

	a->mapped = false;
	dma_unmap_sgtable(a->dev, table, direction, 0);
}

static int
carveout_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
				       enum dma_data_direction direction)
{
	struct carveout_heap_buffer_priv *priv = dmabuf->priv;
	struct carveout_heap_attachment *a;

	mutex_lock(&priv->lock);

	list_for_each_entry(a, &priv->attachments, head) {
		if (!a->mapped)
			continue;

		dma_sync_sgtable_for_cpu(a->dev, &a->table, direction);
	}

	mutex_unlock(&priv->lock);

	return 0;
}

static int
carveout_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				     enum dma_data_direction direction)
{
	struct carveout_heap_buffer_priv *priv = dmabuf->priv;
	struct carveout_heap_attachment *a;

	mutex_lock(&priv->lock);

	list_for_each_entry(a, &priv->attachments, head) {
		if (!a->mapped)
			continue;

		dma_sync_sgtable_for_device(a->dev, &a->table, direction);
	}

	mutex_unlock(&priv->lock);

	return 0;
}

static int carveout_heap_mmap(struct dma_buf *dmabuf,
			      struct vm_area_struct *vma)
{
	struct carveout_heap_buffer_priv *priv = dmabuf->priv;
	struct page *page = virt_to_page(priv->buffer);

	return remap_pfn_range(vma, vma->vm_start, page_to_pfn(page),
			       priv->num_pages * PAGE_SIZE, vma->vm_page_prot);
}

static void carveout_heap_dma_buf_release(struct dma_buf *buf)
{
	struct carveout_heap_buffer_priv *buffer_priv = buf->priv;
	struct carveout_heap_priv *heap_priv = buffer_priv->heap;

	gen_pool_free(heap_priv->pool, (unsigned long)buffer_priv->buffer,
		      buffer_priv->len);
	kfree(buffer_priv);
}

static const struct dma_buf_ops carveout_heap_buf_ops = {
	.attach		= carveout_heap_attach,
	.detach		= carveout_heap_detach,
	.map_dma_buf	= carveout_heap_map_dma_buf,
	.unmap_dma_buf	= carveout_heap_unmap_dma_buf,
	.begin_cpu_access	= carveout_heap_dma_buf_begin_cpu_access,
	.end_cpu_access	= carveout_heap_dma_buf_end_cpu_access,
	.mmap		= carveout_heap_mmap,
	.release	= carveout_heap_dma_buf_release,
};

static struct dma_buf *carveout_heap_allocate(struct dma_heap *heap,
					      unsigned long len,
					      unsigned long fd_flags,
					      unsigned long heap_flags)
{
	struct carveout_heap_priv *heap_priv = dma_heap_get_drvdata(heap);
	struct carveout_heap_buffer_priv *buffer_priv;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *buf;
	dma_addr_t daddr;
	void *buffer;
	int ret;

	buffer_priv = kzalloc(sizeof(*buffer_priv), GFP_KERNEL);
	if (!buffer_priv)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer_priv->attachments);
	mutex_init(&buffer_priv->lock);

	buffer = gen_pool_dma_zalloc(heap_priv->pool, len, &daddr);
	if (!buffer) {
		ret = -ENOMEM;
		goto err_free_buffer_priv;
	}

	buffer_priv->buffer = buffer;
	buffer_priv->heap = heap_priv;
	buffer_priv->len = len;
	buffer_priv->num_pages = len / PAGE_SIZE;

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &carveout_heap_buf_ops;
	exp_info.size = buffer_priv->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer_priv;

	buf = dma_buf_export(&exp_info);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_free_buffer;
	}

	return buf;

err_free_buffer:
	gen_pool_free(heap_priv->pool, (unsigned long)buffer, len);
err_free_buffer_priv:
	kfree(buffer_priv);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops carveout_heap_ops = {
	.allocate = carveout_heap_allocate,
};

static int __init carveout_heap_setup(struct device_node *node)
{
	struct dma_heap_export_info exp_info = {};
	const struct reserved_mem *rmem;
	struct carveout_heap_priv *priv;
	struct dma_heap *heap;
	struct gen_pool *pool;
	void *base;
	int ret;

	rmem = of_reserved_mem_lookup(node);
	if (!rmem)
		return -EINVAL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	pool = gen_pool_create(PAGE_SHIFT, NUMA_NO_NODE);
	if (!pool) {
		ret = -ENOMEM;
		goto err_cleanup_heap;
	}
	priv->pool = pool;

	base = memremap(rmem->base, rmem->size, MEMREMAP_WB);
	if (!base) {
		ret = -ENOMEM;
		goto err_release_mem_region;
	}

	ret = gen_pool_add_virt(pool, (unsigned long)base, rmem->base,
				rmem->size, NUMA_NO_NODE);
	if (ret)
		goto err_unmap;

	exp_info.name = node->full_name;
	exp_info.ops = &carveout_heap_ops;
	exp_info.priv = priv;

	heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap)) {
		ret = PTR_ERR(heap);
		goto err_cleanup_pool_region;
	}
	priv->heap = heap;

	return 0;

err_cleanup_pool_region:
	gen_pool_free(pool, (unsigned long)base, rmem->size);
err_unmap:
	memunmap(base);
err_release_mem_region:
	gen_pool_destroy(pool);
err_cleanup_heap:
	kfree(priv);
	return ret;
}

static int __init carveout_heap_init(void)
{
	struct device_node *rmem_node;
	struct device_node *node;
	int ret;

	rmem_node = of_find_node_by_path("/reserved-memory");
	if (!rmem_node)
		return 0;

	for_each_child_of_node(rmem_node, node) {
		if (!of_property_read_bool(node, "export"))
			continue;

		ret = carveout_heap_setup(node);
		if (ret)
			return ret;
	}

	return 0;
}

module_init(carveout_heap_init);
