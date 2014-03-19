#include <GL/gl.h>
#include <stdbool.h>
#include <v3d2.h>
#include <stdio.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mailbox.h"
#include "compiler.h"

typedef struct vertex {
	uint16_t x,y;
	float z,w,u,v,dummy;
} vertex;

typedef struct v3dTexture {
} v3dTexture;

#define MAXPOINT 60000
#define MAXINDEX 40000

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

vertex *points;
vertex *nextVertex;

int nextpoint = 0;

float texturex,texturey;

uint16_t *primitives = 0;
int nextprimitive = 0;
struct V3D2MemoryReference *vertexData,*shaderCode,*tileAllocation,*tileState = 0,*primitiveList,*textureData;

// should be local to a display list
#define MAXTEX 10
v3dTexture *textures[MAXTEX];
v3dTexture *active2D;

float r,g,b,a;
int tilewidth,tileheight;
int lastwidth,lastheight,mbox;
DISPMANX_RESOURCE_HANDLE_T resource1 = 0;
DISPMANX_DISPLAY_HANDLE_T display;
DISPMANX_MODEINFO_T info;
int displayed = 0;
DISPMANX_ELEMENT_HANDLE_T element = 0;

struct V3D2MemoryReference *makeShader1Code() {
	struct V3D2MemoryReference *shaderCode = V3D2Allocate(0x50);
	uint8_t *shadervirt = (uint8_t*)V3D2mmap(shaderCode);
	uint8_t *p = shadervirt;
	addword(&p, 0x958e0dbf);
	addword(&p, 0xd1724823); /* mov r0, vary; mov r3.8d, 1.0 */
	addword(&p, 0x818e7176);
	addword(&p, 0x40024821); /* fadd r0, r0, r5; mov r1, vary */
	addword(&p, 0x818e7376);
	addword(&p, 0x10024862); /* fadd r1, r1, r5; mov r2, vary */
	addword(&p, 0x819e7540);
	addword(&p, 0x114248a3); /* fadd r2, r2, r5; mov r3.8a, r0 */
	addword(&p, 0x809e7009);
	addword(&p, 0x115049e3); /* nop; mov r3.8b, r1 */
	addword(&p, 0x809e7012);
	addword(&p, 0x116049e3); /* nop; mov r3.8c, r2 */
	addword(&p, 0x159e76c0);
	addword(&p, 0x30020ba7); /* mov tlbc, r3; nop; thrend */
	addword(&p, 0x009e7000);
	addword(&p, 0x100009e7); /* nop; nop; nop */
	addword(&p, 0x009e7000);
	addword(&p, 0x500009e7); /* nop; nop; sbdone */
	assert((p - shadervirt) < 0x50);
	V3D2munmap(shaderCode);
	return shaderCode;
}
struct V3D2MemoryReference *makeShader2Code() {
	struct stat buf;
	int ret;
	FILE *fp;
	struct V3D2MemoryReference *shaderCode;
	char *path = "/media/videos/4tb/rpi/videocoreiv-qpu/qpu-tutorial/texture.raw";
	uint8_t *shadervirt;
	
	ret = stat(path,&buf);
	assert(ret == 0);
	shaderCode = V3D2Allocate(buf.st_size);
	shadervirt = (uint8_t*)V3D2mmap(shaderCode);
	fp = fopen(path,"rb");
	assert(fp);
	fread(shadervirt,1,buf.st_size,fp);
	fclose(fp);
	V3D2munmap(shaderCode);
	return shaderCode;
}
struct V3D2MemoryReference *loadTexture() {
	struct stat buf;
	int ret;
	FILE *fp;
	struct V3D2MemoryReference *buffer;
	char *path = "/home/pi/arial.tformat";
	uint8_t *virt;
	
	ret = stat(path,&buf);
	printf("%d %d\n",ret,errno);
	if (ret) puts(strerror(errno));
	assert(ret == 0);
	printf("allocating %d bytes\n",buf.st_size);
	buffer = V3D2Allocate(buf.st_size);
	virt = (uint8_t*)V3D2mmap(buffer);
	fp = fopen(path,"rb");
	assert(fp);
	fread(virt,1,buf.st_size,fp);
	fclose(fp);
	V3D2munmap(buffer);
	return buffer;
}
void initDispman() {
	int ret;
	uint32_t vc_image_ptr;

	bcm_host_init();
	display = vc_dispmanx_display_open(0);
	puts("getting info");
	ret = vc_dispmanx_display_get_info( display, &info);
	assert(ret == 0);
	printf( "Display is %d x %d\n", info.width, info.height );
	//VC_DISPMANX_ALPHA_T alpha;
	//alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
	//alpha.opacity = 120;
	//alpha.mask = 0;
}
DISPMANX_RESOURCE_HANDLE_T makeResource(int width, int height) {
	DISPMANX_RESOURCE_HANDLE_T resource;
	VC_IMAGE_TYPE_T type = VC_IMAGE_RGBA32;
	uint32_t vc_image_ptr;
	//uint8_t *data = malloc(width*height*4);
	//VC_RECT_T dst_rect;
	//int ret;
	//assert(data);
	
	resource = vc_dispmanx_resource_create( type,width,height,&vc_image_ptr );
	assert( resource );
	
	//memset(data,0x80,width*height*4);
	
	//vc_dispmanx_rect_set(&dst_rect,0,0,width,height);
	//ret = vc_dispmanx_resource_write_data(resource,type,ALIGN_UP(width*4,32),data,&dst_rect);
	//assert(ret==0);
	//free(data);
	return resource;
}
DISPMANX_MODEINFO_T *initOpenGl() {
	// FIXME, where should this go?
	int i;
	v3d2_init();
	initDispman();
	mbox = mbox_open(); // FIXME, needs root, remove when possible
	
	vertexData = V3D2Allocate(24 * MAXPOINT);
	assert(vertexData);
	points = V3D2mmap(vertexData);
	assert(points);
	nextVertex = points;

	shaderCode = makeShader2Code();
	textureData = loadTexture();
	tileAllocation = V3D2Allocate(0x10000);
	
	primitiveList = V3D2Allocate(sizeof(uint16_t) * MAXINDEX);
	primitives = V3D2mmap(primitiveList);
	printf("primitives at %p\n",primitives);

	for (i=0; i<MAXTEX; i++) textures[i] = NULL;
	return &info;
}
void stopOpenGl() {
	// FIXME, where should this go?
	V3D2Free(shaderCode);
	V3D2Free(textureData);
	V3D2munmap(vertexData);
	V3D2Free(vertexData);
	V3D2Free(tileAllocation);
	if (tileState) V3D2Free(tileState);
	V3D2munmap(primitiveList);
	primitives = 0;
	V3D2Free(primitiveList);
	puts("released primitives");
}
void displaySurface(DISPMANX_RESOURCE_HANDLE_T res,int width, int height) {
	DISPMANX_UPDATE_HANDLE_T update;
	VC_RECT_T dst_rect,src_rect;
	int ret;
	
	update = vc_dispmanx_update_start( 10 );
	assert(update);
	vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16 );
	vc_dispmanx_rect_set( &dst_rect, ( info.width - width ) / 2,
		( info.height - height ) / 2,
		width,
		height );
	if (displayed == 0) {
		element = vc_dispmanx_element_add(update,display,2000,&dst_rect,res,&src_rect,DISPMANX_PROTECTION_NONE,NULL,NULL,DISPMANX_NO_ROTATE);
		displayed = 1;
	}
	ret = vc_dispmanx_update_submit_sync( update );
	assert( ret == 0 );
}
void setViewportSize(int width, int height) {
#define DIV_CIEL(x,y) ( ((x)+(y-1)) / y)
	tilewidth = DIV_CIEL(width,64);
	tileheight = DIV_CIEL(height,64);
	
	lastwidth = width;
	lastheight = height;

	resource1 = makeResource(width,height);
	displaySurface(resource1,width,height);
	if (tileState) V3D2Free(tileState);
	tileState = V3D2Allocate(48 * tilewidth * tileheight);
}
GLAPI void GLAPIENTRY glVertex2f( GLfloat x, GLfloat y ){
	// FIXME
	int x2 = x;
	int y2 = y;
	glVertex2s(x2,y2);
}
GLAPI void GLAPIENTRY glVertex2s( GLshort x, GLshort y ){
	static int a,b,c,d;
	static int corner = 0;
	if (nextprimitive > (MAXINDEX-50)) return;
	
	assert(nextpoint < MAXPOINT);
	nextVertex->x = x << 4;
	nextVertex->y = y << 4;
	nextVertex->z = 1;
	nextVertex->w = 1;
	nextVertex->u = texturex; // these refer to the texture in active2D
	nextVertex->v = texturey;
	nextVertex->dummy = 0;
	nextVertex++;
	
	// FIXME, optimize this
	switch (corner) {
	case 0:
		a = nextpoint;
		break;
	case 1:
		b = nextpoint;
		break;
	case 2:
		c = nextpoint;
		break;
	case 3:
		d = nextpoint;
		primitives[nextprimitive++] = a;
		primitives[nextprimitive++] = b;
		primitives[nextprimitive++] = c;
		primitives[nextprimitive++] = c;
		primitives[nextprimitive++] = d;
		primitives[nextprimitive++] = a;
		corner = -1;
		break;
	}
	corner++;
	nextpoint++;
}
GLAPI void GLAPIENTRY glTexCoord2f( GLfloat s, GLfloat t ){
	texturex = s;
	texturey = t;
	int t1,t2;
	t1 = s * 256;
	t2 = t*256;
	//printf("%d,%d\n",t1,t2);
}
struct V3D2MemoryReference *makeShaderRecord(struct V3D2MemoryReference *shaderCode,
	struct V3D2MemoryReference *shaderUniforms,
	struct V3D2MemoryReference *vertexData) {

	struct V3D2MemoryReference *shader = V3D2Allocate(0x20);
	uint8_t *shadervirt = (uint8_t*)V3D2mmap(shader);
	uint8_t *p = shadervirt;
	addbyte(&p, 0x01); // flags
	addbyte(&p, 6*4); // stride FIXME, must match size of struct vertex
	addbyte(&p, 0x2); // num uniforms (not used)
	addbyte(&p, 2); // num varyings
	addword(&p, shaderCode->phys); // Fragment shader code
	addword(&p, shaderUniforms->phys); // Fragment shader uniforms
	addword(&p, vertexData->phys); // Vertex Data
	assert((p - shadervirt) < 0x20);
	V3D2munmap(shader);
	return shader;
}
uint8_t *makeBinner(struct V3D2MemoryReference *tileAllocation,uint32_t tileAllocationSize,
		struct V3D2MemoryReference *tileState, struct V3D2MemoryReference *shaderRecordAddress,
		struct V3D2MemoryReference *primitiveIndexAddress, int *sizeOut, int width, int height,
		uint16_t primitiveCount,uint32_t maxindex) {
	uint8_t *binner = malloc(0x80);
	uint8_t *p = binner;
	int length;
	// Configuration stuff
	// Tile Binning Configuration.
	//   Tile state data is 48 bytes per tile, I think it can be thrown away
	//   as soon as binning is finished.
	addbyte(&p, 112);
	addword(&p, tileAllocation->phys); // tile allocation memory address
	addword(&p, tileAllocationSize); // tile allocation memory size
	addword(&p, tileState->phys); // Tile state data address
	addbyte(&p, tilewidth); // 1920/64
	addbyte(&p, tileheight); // 1080/64 (16.875)
	addbyte(&p, 0x04); // config, sets tile allocation size to 32bytes per tile

	// Start tile binning.
	addbyte(&p, 6);

	// Primitive type
	addbyte(&p, 56);
	addbyte(&p, 0x32); // 16 bit triangle

	// Clip Window
	addbyte(&p, 102);
	addshort(&p, 0);
	addshort(&p, 0);
	addshort(&p, width); // width
	addshort(&p, height); // height

	// State
	addbyte(&p, 96);
	addbyte(&p, 0x03); // enable both foward and back facing polygons
	addbyte(&p, 0x00); // depth testing disabled
	addbyte(&p, 0x02); // enable early depth write

	// Viewport offset
	addbyte(&p, 103);
	addshort(&p, 0);
	addshort(&p, 0);

	// The triangle
	// No Vertex Shader state (takes pre-transformed vertexes, 
	// so we don't have to supply a working coordinate shader to test the binner.
	addbyte(&p, 65);
	addword(&p, shaderRecordAddress->phys); // Shader Record

	// primitive index list
	addbyte(&p, 32);
	addbyte(&p, 0x14); // 16bit index, triangles
	addword(&p, primitiveCount); // Length
	addword(&p, primitiveIndexAddress->phys); // address
	addword(&p, maxindex); // Maximum index

	// End of bin list
	// Flush
	addbyte(&p, 5);
	// Nop
	addbyte(&p, 1);
	// Nop
	addbyte(&p, 1);

	length = p - binner;
	*sizeOut = length;
	assert(length < 0x80);
	return binner;
}
unsigned int get_dispman_handle(int file_desc, unsigned int handle) {
	// TODO: move this into the kernel
	int i=0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request
	
	p[i++] = 0x30014; // (the tag id)
	p[i++] = 8; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;
	p[i++] = 0; // filler
	
	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size
	
	mbox_property(file_desc, p);
	//printf("success %d, handle %x\n",p[5],p[6]);
	return p[6];
}
uint8_t *makeRenderer(uint32_t outputFrame, struct V3D2MemoryReference *tileAllocationAddress,
		int *sizeOut, int width, int height) {
	uint32_t x,y;
	uint8_t *render = malloc(0x2000);
	memset(render,0x66,0x2000);
	uint8_t *p = render;
	// Render control list
	
	// Clear color
	addbyte(&p, 114);
	addword(&p, 0x00000000); // transparent Black
	addword(&p, 0x00000000); // 32 bit clear colours need to be repeated twice
	addword(&p, 0); // clear zs and clear vg mask
	addbyte(&p, 0); // clear stencil
	
	// Tile Rendering Mode Configuration
	// linear rgba8888 == VC_IMAGE_RGBA32
	// t-format rgba8888 = VC_IMAGE_TF_RGBA32
	addbyte(&p, 113);
	addword(&p, outputFrame);	//  0->31 framebuffer addresss
	addshort(&p, width);		// 32->47 width
	addshort(&p, height);		// 48->63 height
	addbyte(&p, 0x4);		// 64 multisample mpe
					// 65 tilebuffer depth
					// 66->67 framebuffer mode (linear rgba8888)
					// 68->69 decimate mode
					// 70->71 memory format
	addbyte(&p, 0x00); // vg mask, coverage mode, early-z update, early-z cov, double-buffer
	
	// Do a store of the first tile to force the tile buffer to be cleared
	// Tile Coordinates
	addbyte(&p, 115);
	addbyte(&p, 0);
	addbyte(&p, 0);
	
	// Store Tile Buffer General
	addbyte(&p, 28);
	addshort(&p, 0); // Store nothing (just clear)
	addword(&p, 0); // no address is needed
	
	// Link all binned lists together
	for(y = 0; y < tileheight; y++) {
		for(x = 0; x < tilewidth; x++) { 
			// Tile Coordinates
			addbyte(&p, 115);
			addbyte(&p, x);
			addbyte(&p, y);
			
			// Call Tile sublist
			addbyte(&p, 17);
			uint32_t addr = tileAllocationAddress->phys + (y * tilewidth + x) * 32;
			//printf("x:%2d y:%2d addr:%8p tilewidth:%d base:%8x\n",x,y,addr,tilewidth,tileAllocationAddress->phys);
			addword(&p, addr); // 2d array of 32byte objects
			
			// Last tile needs a special store instruction
			if ((x == (tilewidth-1)) && (y == (tileheight-1))) {
				// Store resolved tile color buffer and signal end of frame
				addbyte(&p, 25);
			} else {
				// Store resolved tile color buffer
				addbyte(&p, 24);
			}
		}
	}
	int size = p - render;
	*sizeOut = size;
	//printf("render size %d\n",size);
	assert(size < 0x2000); // FIXME, if this triggers, its already overflown its buffer
	return render;
}
void glFlush(void) {
	int binnerSize,renderSize,dispman_mem_handle;
	struct V3D2MemoryReference *shaderUniforms = V3D2Allocate(0x10);
	struct V3D2MemoryReference *shaderRecord = makeShaderRecord(shaderCode,shaderUniforms,vertexData);
	uint8_t *binner,*render;
	JobCompileRequest job;
	struct JobStatusPacket status;
	int ret,index;
	uint32_t rawdispman;
	uint8_t *temp;
	
	assert(tileState);
	assert(tileAllocation);

	uint32_t *uniformvirt = (uint32_t*)V3D2mmap(shaderUniforms);
	uint32_t textaddr = textureData->phys & 0xfffffff;
	assert((textaddr & 0xfff) == 0);
	uniformvirt[0] = textaddr;
	uniformvirt[1] = (256 << 8) | (256<<20);
	printf("config 0 %x\nconfig1 %x\n",uniformvirt[0],uniformvirt[1]);
	uniformvirt[2] = 0;
	V3D2munmap(shaderUniforms);


	dispman_mem_handle = get_dispman_handle(mbox,resource1);
	rawdispman = mem_lock(mbox,dispman_mem_handle);	
	
	binner = makeBinner(tileAllocation,0x10000,tileState,shaderRecord,
			primitiveList,&binnerSize,lastwidth,lastheight,nextprimitive,nextpoint-1);

	render = makeRenderer(rawdispman,tileAllocation,&renderSize,
			lastwidth,lastheight);

	/*printf("should render %d vertexes in %d primitive points\n",nextpoint,nextprimitive);
	for (ret = 0; ret < 12; ret++) {
		index = primitives[ret];
		printf("primitive %d points to vertex %d (%d,%d)\n",ret,index,points[index].x >> 4,points[index].y >> 4);
	}*/
		memset(&job,0,sizeof(job));
		job.binner.code = binner;
		job.binner.size = binnerSize;
		job.binner.run = 1;
		job.renderer.code = render;
		job.renderer.size = renderSize;
		job.renderer.run = 1;
		job.jobid = 123;
		job.outputType = opMemoryHandle;
		status.jobid = 0;
	printf("b\n");
		ret = ioctl(v3d2_get_fd(),V3D2_COMPILE_CL,&job);
		if (ret) {
			printf("error %d %s from V3D2_COMPILE_CL\n",errno,strerror(errno));
	} else {
		read(v3d2_get_fd(),&status,sizeof(status));
		//printf("done frame %d\n",status.jobid);
	}
	printf("c\n");
	/*printf("tileAllocation %d == ",tileAllocation->size);
	temp = V3D2mmap(tileAllocation);
	assert(temp);
	for (ret=0; ret<tileAllocation->size; ret++) {
		if ((ret % 32) == 0) printf("\n %4d\t",ret);
		printf("%03d ",temp[ret]);
		if (ret > 0x1000) break;
	}
	V3D2munmap(tileAllocation);
	printf("\n");
	assert(0);*/
	
	mem_unlock(mbox,dispman_mem_handle);
	printf("d\n");
	
	nextpoint = 0;
	nextprimitive = 0;
	nextVertex = points;
	free(binner);
	V3D2Free(shaderUniforms);
	V3D2Free(shaderRecord);
}
void glBindTexture(GLenum target, GLuint texture) {
	switch (target) {
	case GL_TEXTURE_2D:
		active2D = textures[texture];
		break;
	default:
		assert(0);
		puts(__func__);
	}
}
void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
	r = red;
	g = green;
	b = blue;
	a = alpha;
}
GLAPI void GLAPIENTRY glGetFloatv( GLenum pname, GLfloat *params ){
	switch (pname) {
	case GL_CURRENT_COLOR:
		params[0] = r;
		params[1] = g;
		params[2] = b;
		params[3] = a;
		break;
	default:
		assert(0);
	}
}
void glGenTextures(GLsizei n, GLuint *out) {
	int i;

	puts(__func__);
	for (i=1; i<MAXTEX; i++) {
		if (textures[i] == 0) {
			textures[i] = malloc(sizeof(v3dTexture));
			*out = i;
			n--;
			out++;
		}
		if (n <= 0) return;
	}
}
void glBegin(GLenum mode) {
	assert(mode == GL_QUADS);
}

void glEnd(void) {
}

