/* ASHSPORE march, drawn with the Vulkan API.
   One offscreen frame: crossed roads, your ashbed, their site, wild sites.
   No window is required. SwiftShader or lavapipe both present it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vulkan/vulkan.h>
#include "shaders.h"

#define W 960
#define H 540
#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { fprintf(stderr, "%s -> %d\n", #x, (int)_r); return 1; } } while (0)

typedef struct { float x, y, r, g, b, pad; } Vert;

static uint32_t memType(VkPhysicalDevice gpu, uint32_t bits, VkMemoryPropertyFlags need) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(gpu, &props);
  for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
    if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & need) == need) return i;
  }
  return 0xffffffffu;
}

static void push(Vert *v, int *n, int cap, float x, float y, float r, float g, float b) {
  if (*n >= cap) return;
  v[*n].x = x;
  v[*n].y = y;
  v[*n].r = r;
  v[*n].g = g;
  v[*n].b = b;
  v[*n].pad = 0;
  *n += 1;
}

static void tri(Vert *v, int *n, int cap, float ax, float ay, float bx, float by, float cx, float cy, float r, float g, float b) {
  push(v, n, cap, ax, ay, r, g, b);
  push(v, n, cap, bx, by, r, g, b);
  push(v, n, cap, cx, cy, r, g, b);
}

static void disc(Vert *v, int *n, int cap, float x, float y, float rad, float r, float g, float b) {
  const int slices = 12;
  for (int i = 0; i < slices; i++) {
    float a0 = (float)i / slices * 6.2831853f;
    float a1 = (float)(i + 1) / slices * 6.2831853f;
    tri(v, n, cap, x, y, x + cosf(a0) * rad, y + sinf(a0) * rad, x + cosf(a1) * rad, y + sinf(a1) * rad, r, g, b);
  }
}

static void road(Vert *v, int *n, int cap, float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.0001f) return;
  float nx = -dy / len * 0.012f;
  float ny = dx / len * 0.012f;
  tri(v, n, cap, x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, 0.45f, 0.32f, 0.16f);
  tri(v, n, cap, x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, 0.45f, 0.32f, 0.16f);
}

int main(void) {
  VkApplicationInfo app = {0};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "ASHSPORE";
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo ici = {0};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;
  VkInstance instance;
  CHECK(vkCreateInstance(&ici, NULL, &instance));

  uint32_t ngpu = 0;
  CHECK(vkEnumeratePhysicalDevices(instance, &ngpu, NULL));
  if (!ngpu) { fprintf(stderr, "no vulkan device\n"); return 1; }
  ngpu = 1;
  VkPhysicalDevice gpu;
  CHECK(vkEnumeratePhysicalDevices(instance, &ngpu, &gpu));
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(gpu, &props);
  printf("vulkan device: %s api %u.%u.%u\n", props.deviceName,
    VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));

  uint32_t nq = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &nq, NULL);
  VkQueueFamilyProperties *qprops = calloc(nq, sizeof *qprops);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &nq, qprops);
  uint32_t family = 0xffffffffu;
  for (uint32_t i = 0; i < nq; i++) if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { family = i; break; }
  free(qprops);
  if (family == 0xffffffffu) { fprintf(stderr, "no graphics queue\n"); return 1; }

  float prio = 1.f;
  VkDeviceQueueCreateInfo qci = {0};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = family;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  VkDeviceCreateInfo dci = {0};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  VkDevice device;
  CHECK(vkCreateDevice(gpu, &dci, NULL, &device));
  VkQueue queue;
  vkGetDeviceQueue(device, family, 0, &queue);

  VkImageCreateInfo imi = {0};
  imi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imi.imageType = VK_IMAGE_TYPE_2D;
  imi.format = VK_FORMAT_R8G8B8A8_UNORM;
  imi.extent.width = W;
  imi.extent.height = H;
  imi.extent.depth = 1;
  imi.mipLevels = 1;
  imi.arrayLayers = 1;
  imi.samples = VK_SAMPLE_COUNT_1_BIT;
  imi.tiling = VK_IMAGE_TILING_OPTIMAL;
  imi.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  imi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image;
  CHECK(vkCreateImage(device, &imi, NULL, &image));
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device, image, &req);
  uint32_t imgMem = memType(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (imgMem == 0xffffffffu) imgMem = memType(gpu, req.memoryTypeBits, 0);
  VkMemoryAllocateInfo mai = {0};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = imgMem;
  VkDeviceMemory imageMemory;
  CHECK(vkAllocateMemory(device, &mai, NULL, &imageMemory));
  CHECK(vkBindImageMemory(device, image, imageMemory, 0));

  VkImageViewCreateInfo ivi = {0};
  ivi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  ivi.image = image;
  ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  ivi.format = VK_FORMAT_R8G8B8A8_UNORM;
  ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  ivi.subresourceRange.levelCount = 1;
  ivi.subresourceRange.layerCount = 1;
  VkImageView view;
  CHECK(vkCreateImageView(device, &ivi, NULL, &view));

  VkAttachmentDescription att = {0};
  att.format = VK_FORMAT_R8G8B8A8_UNORM;
  att.samples = VK_SAMPLE_COUNT_1_BIT;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub = {0};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &ref;
  VkRenderPassCreateInfo rpi = {0};
  rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpi.attachmentCount = 1;
  rpi.pAttachments = &att;
  rpi.subpassCount = 1;
  rpi.pSubpasses = &sub;
  VkRenderPass pass;
  CHECK(vkCreateRenderPass(device, &rpi, NULL, &pass));

  VkFramebufferCreateInfo fbi = {0};
  fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbi.renderPass = pass;
  fbi.attachmentCount = 1;
  fbi.pAttachments = &view;
  fbi.width = W;
  fbi.height = H;
  fbi.layers = 1;
  VkFramebuffer fb;
  CHECK(vkCreateFramebuffer(device, &fbi, NULL, &fb));

  VkShaderModuleCreateInfo smi = {0};
  smi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  smi.codeSize = sizeof(vert_spv);
  smi.pCode = vert_spv;
  VkShaderModule vs, fs;
  CHECK(vkCreateShaderModule(device, &smi, NULL, &vs));
  smi.codeSize = sizeof(frag_spv);
  smi.pCode = frag_spv;
  CHECK(vkCreateShaderModule(device, &smi, NULL, &fs));

  VkPipelineShaderStageCreateInfo stages[2] = {0};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fs;
  stages[1].pName = "main";

  VkVertexInputBindingDescription bind = {0, sizeof(Vert), VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription attrs[2] = {
    {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 8},
  };
  VkPipelineVertexInputStateCreateInfo vi = {0};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &bind;
  vi.vertexAttributeDescriptionCount = 2;
  vi.pVertexAttributeDescriptions = attrs;
  VkPipelineInputAssemblyStateCreateInfo ia = {0};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkViewport viewport = {0, 0, W, H, 0, 1};
  VkRect2D scissor = {{0, 0}, {W, H}};
  VkPipelineViewportStateCreateInfo vp = {0};
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.viewportCount = 1;
  vp.pViewports = &viewport;
  vp.scissorCount = 1;
  vp.pScissors = &scissor;
  VkPipelineRasterizationStateCreateInfo rs = {0};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.f;
  VkPipelineMultisampleStateCreateInfo ms = {0};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState blend = {0};
  blend.colorWriteMask = 0xf;
  VkPipelineColorBlendStateCreateInfo cb = {0};
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.attachmentCount = 1;
  cb.pAttachments = &blend;
  VkPipelineLayoutCreateInfo pli = {0};
  pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  VkPipelineLayout layout;
  CHECK(vkCreatePipelineLayout(device, &pli, NULL, &layout));
  VkGraphicsPipelineCreateInfo gpi = {0};
  gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gpi.stageCount = 2;
  gpi.pStages = stages;
  gpi.pVertexInputState = &vi;
  gpi.pInputAssemblyState = &ia;
  gpi.pViewportState = &vp;
  gpi.pRasterizationState = &rs;
  gpi.pMultisampleState = &ms;
  gpi.pColorBlendState = &cb;
  gpi.layout = layout;
  gpi.renderPass = pass;
  VkPipeline pipe;
  CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));

  Vert *verts = calloc(8000, sizeof *verts);
  int count = 0;
  tri(verts, &count, 8000, -1, -1, 1, -1, -1, 1, 0.10f, 0.16f, 0.09f);
  tri(verts, &count, 8000, -1, 1, 1, -1, 1, 1, 0.16f, 0.22f, 0.10f);
  float sites[8][2] = {{-0.72f,-0.35f},{-0.35f,0.15f},{0.02f,-0.2f},{0.28f,0.32f},{0.55f,-0.05f},{0.78f,0.28f},{-0.1f,0.45f},{0.4f,-0.48f}};
  int roads[][2] = {{0,1},{1,2},{2,3},{2,4},{4,5},{1,6},{4,7},{3,5},{0,2},{6,3}};
  for (int i = 0; i < 10; i++) road(verts, &count, 8000, sites[roads[i][0]][0], sites[roads[i][0]][1], sites[roads[i][1]][0], sites[roads[i][1]][1]);
  disc(verts, &count, 8000, sites[0][0], sites[0][1], 0.055f, 0.95f, 0.45f, 0.18f);
  disc(verts, &count, 8000, sites[5][0], sites[5][1], 0.055f, 0.85f, 0.22f, 0.16f);
  for (int i = 1; i < 8; i++) if (i != 5) disc(verts, &count, 8000, sites[i][0], sites[i][1], 0.04f, 0.78f, 0.70f, 0.52f);

  VkDeviceSize vbytes = (VkDeviceSize)count * sizeof(Vert);
  VkBufferCreateInfo bci = {0};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size = vbytes;
  bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  VkBuffer vbuf;
  CHECK(vkCreateBuffer(device, &bci, NULL, &vbuf));
  vkGetBufferMemoryRequirements(device, vbuf, &req);
  uint32_t host = memType(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = host;
  VkDeviceMemory vmem;
  CHECK(vkAllocateMemory(device, &mai, NULL, &vmem));
  CHECK(vkBindBufferMemory(device, vbuf, vmem, 0));
  void *mapped = NULL;
  CHECK(vkMapMemory(device, vmem, 0, vbytes, 0, &mapped));
  memcpy(mapped, verts, (size_t)vbytes);
  vkUnmapMemory(device, vmem);

  VkDeviceSize pixels = (VkDeviceSize)W * H * 4;
  bci.size = pixels;
  bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer readback;
  CHECK(vkCreateBuffer(device, &bci, NULL, &readback));
  vkGetBufferMemoryRequirements(device, readback, &req);
  host = memType(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = host;
  VkDeviceMemory rmem;
  CHECK(vkAllocateMemory(device, &mai, NULL, &rmem));
  CHECK(vkBindBufferMemory(device, readback, rmem, 0));

  VkCommandPoolCreateInfo pci = {0};
  pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pci.queueFamilyIndex = family;
  VkCommandPool pool;
  CHECK(vkCreateCommandPool(device, &pci, NULL, &pool));
  VkCommandBufferAllocateInfo cai = {0};
  cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cai.commandPool = pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  VkCommandBuffer cmd;
  CHECK(vkAllocateCommandBuffers(device, &cai, &cmd));
  VkCommandBufferBeginInfo begin = {0};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CHECK(vkBeginCommandBuffer(cmd, &begin));
  VkClearValue clear = {{{0.05f, 0.07f, 0.04f, 1.f}}};
  VkRenderPassBeginInfo rp = {0};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = pass;
  rp.framebuffer = fb;
  rp.renderArea.extent.width = W;
  rp.renderArea.extent.height = H;
  rp.clearValueCount = 1;
  rp.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
  VkDeviceSize zero = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &zero);
  vkCmdDraw(cmd, (uint32_t)count, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
  VkBufferImageCopy copy = {0};
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.layerCount = 1;
  copy.imageExtent.width = W;
  copy.imageExtent.height = H;
  copy.imageExtent.depth = 1;
  vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
  CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo submit = {0};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
  CHECK(vkQueueWaitIdle(queue));

  unsigned char *rgba = NULL;
  CHECK(vkMapMemory(device, rmem, 0, pixels, 0, (void **)&rgba));
  FILE *out = fopen("ashspore.ppm", "wb");
  if (!out) { perror("ashspore.ppm"); return 1; }
  fprintf(out, "P6\n%d %d\n255\n", W, H);
  for (int y = H - 1; y >= 0; y--) {
    unsigned char *row = rgba + (size_t)y * W * 4;
    for (int x = 0; x < W; x++) fwrite(row + x * 4, 1, 3, out);
  }
  fclose(out);
  printf("frame: ashspore.ppm  sites 8  roads 10  triangles %d\n", count / 3);
  return 0;
}
