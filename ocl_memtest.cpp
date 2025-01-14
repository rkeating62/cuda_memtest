
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_gl_ext.h>
#include <CL/cl_ext.h>
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include "ocl_tests.h"

extern unsigned int num_iterations;
extern unsigned int num_passes;
unsigned int exit_on_error = 0;

extern cuda_memtest_t cuda_memtests[11];


#define KERNEL_FILE "ocl_memtest_kernels.cpp"

#define MAX_KERNEL_FILE_SIZE (1024*1024)

char hostname[64];
char* kernel_source;

char*
print_cl_errstring(cl_int err)
{
  switch (err) {
  case CL_SUCCESS:                          return strdup("Success!");
  case CL_DEVICE_NOT_FOUND:                 return strdup("Device not found.");
  case CL_DEVICE_NOT_AVAILABLE:             return strdup("Device not available");
  case CL_COMPILER_NOT_AVAILABLE:           return strdup("Compiler not available");
  case CL_MEM_OBJECT_ALLOCATION_FAILURE:    return strdup("Memory object allocation failure");
  case CL_OUT_OF_RESOURCES:                 return strdup("Out of resources");
  case CL_OUT_OF_HOST_MEMORY:               return strdup("Out of host memory");
  case CL_PROFILING_INFO_NOT_AVAILABLE:     return strdup("Profiling information not available");
  case CL_MEM_COPY_OVERLAP:                 return strdup("Memory copy overlap");
  case CL_IMAGE_FORMAT_MISMATCH:            return strdup("Image format mismatch");
  case CL_IMAGE_FORMAT_NOT_SUPPORTED:       return strdup("Image format not supported");
  case CL_BUILD_PROGRAM_FAILURE:            return strdup("Program build failure");
  case CL_MAP_FAILURE:                      return strdup("Map failure");
  case CL_INVALID_VALUE:                    return strdup("Invalid value");
  case CL_INVALID_DEVICE_TYPE:              return strdup("Invalid device type");
  case CL_INVALID_PLATFORM:                 return strdup("Invalid platform");
  case CL_INVALID_DEVICE:                   return strdup("Invalid device");
  case CL_INVALID_CONTEXT:                  return strdup("Invalid context");
  case CL_INVALID_QUEUE_PROPERTIES:         return strdup("Invalid queue properties");
  case CL_INVALID_COMMAND_QUEUE:            return strdup("Invalid command queue");
  case CL_INVALID_HOST_PTR:                 return strdup("Invalid host pointer");
  case CL_INVALID_MEM_OBJECT:               return strdup("Invalid memory object");
  case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:  return strdup("Invalid image format descriptor");
  case CL_INVALID_IMAGE_SIZE:               return strdup("Invalid image size");
  case CL_INVALID_SAMPLER:                  return strdup("Invalid sampler");
  case CL_INVALID_BINARY:                   return strdup("Invalid binary");
  case CL_INVALID_BUILD_OPTIONS:            return strdup("Invalid build options");
  case CL_INVALID_PROGRAM:                  return strdup("Invalid program");
  case CL_INVALID_PROGRAM_EXECUTABLE:       return strdup("Invalid program executable");
  case CL_INVALID_KERNEL_NAME:              return strdup("Invalid kernel name");
  case CL_INVALID_KERNEL_DEFINITION:        return strdup("Invalid kernel definition");
  case CL_INVALID_KERNEL:                   return strdup("Invalid kernel");
  case CL_INVALID_ARG_INDEX:                return strdup("Invalid argument index");
  case CL_INVALID_ARG_VALUE:                return strdup("Invalid argument value");
  case CL_INVALID_ARG_SIZE:                 return strdup("Invalid argument size");
  case CL_INVALID_KERNEL_ARGS:              return strdup("Invalid kernel arguments");
  case CL_INVALID_WORK_DIMENSION:           return strdup("Invalid work dimension");
  case CL_INVALID_WORK_GROUP_SIZE:          return strdup("Invalid work group size");
  case CL_INVALID_WORK_ITEM_SIZE:           return strdup("Invalid work item size");
  case CL_INVALID_GLOBAL_OFFSET:            return strdup("Invalid global offset");
  case CL_INVALID_EVENT_WAIT_LIST:          return strdup("Invalid event wait list");
  case CL_INVALID_EVENT:                    return strdup("Invalid event");
  case CL_INVALID_OPERATION:                return strdup("Invalid operation");
  case CL_INVALID_GL_OBJECT:                return strdup("Invalid OpenGL object");
  case CL_INVALID_BUFFER_SIZE:              return strdup("Invalid buffer size");
  case CL_INVALID_MIP_LEVEL:                return strdup("Invalid mip-map level");
  default:                                  return strdup("Unknown");
  }
}

const char* device_type_str(cl_device_type type)
{
  switch(type){
  case CL_DEVICE_TYPE_DEFAULT:
    return "CL_DEVICE_TYPE_DEFAULT";
  case CL_DEVICE_TYPE_CPU:
    return "CL_DEVICE_TYPE_CPU";
  case CL_DEVICE_TYPE_GPU:
    return "CL_DEVICE_TYPE_GPU";
  case CL_DEVICE_TYPE_ACCELERATOR:
    return "CL_DEVICE_TYPE_ACCELERATOR";
  case CL_DEVICE_TYPE_ALL:
    return "CL_DEVICE_TYPE_ALL";
  default:
    return "UNKNOWN";
  }
  
  return NULL;
}


cl_int oclGetPlatformID(int platform_index, cl_platform_id* clSelectedPlatformID)
{
  char chBuffer[1024];
  cl_uint num_platforms;
  cl_platform_id* clPlatformIDs;
  cl_int rc;
  if(clSelectedPlatformID != NULL){
    *clSelectedPlatformID = NULL;
  }

  // Get OpenCL platform count
  rc = clGetPlatformIDs (0, NULL, &num_platforms); CLERR;

  if(num_platforms == 0){
    printf("ERROR: no OpenCL platform found!\n\n");
    return -3000;
  }
  
  // if there's a platform or more, make space for ID's
  if ((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL){
    printf("ERROR: failed to allocate memory for cl_platform ID's!\n\n");
    return -5000;
  }
  
  // get platform info for each platform and trap the NVIDIA platform if found
  rc = clGetPlatformIDs (num_platforms, clPlatformIDs, NULL); CLERR;

  if(platform_index < 0){
    char strbuf[128];
    printf("\nOpenCL Platforms:\n-----------------\n");
    for(int i = 0; i < num_platforms; ++i){
      rc = clGetPlatformInfo (clPlatformIDs[i], CL_PLATFORM_NAME, sizeof(strbuf), strbuf, NULL); CLERR;
      printf("[%d] Platform: %s, ", i, strbuf);
      rc = clGetPlatformInfo (clPlatformIDs[i], CL_PLATFORM_VERSION, sizeof(strbuf), strbuf, NULL); CLERR;
      printf("Version: %s\n", strbuf);
    }
    printf("\n");
  } else if(platform_index >= num_platforms){
    printf("ERROR: platform-index (%d) greater than max platform-index (%d).\n", platform_index, num_platforms-1);
    return -4000;
  }

  if (platform_index >= 0 && clSelectedPlatformID != NULL){
    *clSelectedPlatformID = clPlatformIDs[platform_index];
  }

  free(clPlatformIDs);
  return CL_SUCCESS;
}

static void
read_kernel_source(void)
{
  kernel_source = (char*)malloc(MAX_KERNEL_FILE_SIZE);
  if(kernel_source == NULL){
    printf("ERROR: malloc failed for kernel source\n");
    exit(1);
  }
  
  FILE* fd = fopen(KERNEL_FILE, "r");
  if (fd == NULL){
    printf("Error: opening kernel file(%s) failed\n", KERNEL_FILE);
    exit(1);
  }
  
  size_t rc = fread(kernel_source, 1, MAX_KERNEL_FILE_SIZE, fd);
  if (rc < 0 || rc >= MAX_KERNEL_FILE_SIZE){
    printf("ERROR: return value out of range for reading kernel file(rc=%lx)\n", rc);
    fclose(fd);
    exit(1);
  }

  fclose(fd);
  return;
}

static int
allocate_device_mem(memtest_control_t* mc)
{
  cl_context context= mc->context;
  cl_device_id device = mc->device;
  cl_command_queue queue = mc->queue;
  cl_int rc;
  
  mc->err_count = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_uint), NULL, &rc);CLERR;
  mc->err_addr = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong)*MAX_ERR_RECORD_COUNT, NULL, &rc); CLERR;
  mc->err_expect = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong)*MAX_ERR_RECORD_COUNT, NULL, &rc); CLERR;
  mc->err_current = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong)*MAX_ERR_RECORD_COUNT, NULL, &rc); CLERR;
  mc->err_second_read = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong)*MAX_ERR_RECORD_COUNT, NULL, &rc); CLERR;
  
  //need set err_count to 0
  cl_ulong err_count= 0;
  rc = clEnqueueWriteBuffer(queue, mc->err_count, CL_TRUE, 0, sizeof(cl_uint), &err_count, 0, NULL, NULL); CLERR;
  
  cl_ulong msize;
  rc = clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(msize), &msize, NULL);
  CLERR;
  
  unsigned long msize_in_mb = msize >>20;
  cl_mem gpu_mem;

  while(msize_in_mb > 0){
    mc->device_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, msize_in_mb*MB, NULL, &rc);
    if (rc == CL_SUCCESS){
      //This does not mean the memory is allocated successfully.
      //let's try to write to the buffer
      int h_data = 1;
      rc = clEnqueueWriteBuffer(queue, mc->device_mem, CL_TRUE, 0, sizeof(int), &h_data, 0, NULL, NULL);
      if (rc == CL_SUCCESS){
	break;
      }
    }
    msize_in_mb --;
  }
  if (mc->device_mem == (cl_mem)0){
    printf("ERROR: allocating memory from device failed\n");
    exit(1);
  }
  
  mc->mem_size = msize_in_mb*MB;
  
  printf("allocated %ld Mbytes from device %d\n", msize_in_mb, mc->device_idx);
  
  return CL_SUCCESS;
}

void list_tests_info(void)
{
    size_t i;
    for (i = 0;i < DIM(cuda_memtests); i++){
	printf("%s %s\n", cuda_memtests[i].desc, cuda_memtests[i].enabled?"":" ==disabled by default==");
    }
    return;
}

void
usage(const char** argv)
{
  printf("usage: %s [options]\n", argv[0]);
  printf("The available options are: \n");
  printf("--device <devid>            device ID, on which the test will perform\n");
  printf("--device_count <n>          number of devices to test (default is all unless device is specified.)\n");
  printf("--platform <n>              index of the OpenCL platform to use (default is 0).\n");
  printf("--list_platforms            list available OpenCL platforms and exit.\n");
  printf("--list_devices              list available OpenCL devices for platform exit.\n");
  printf("--disable_all               Disable all tests\n");
  printf("--enable_test <test_idx>    Enable the test <test_idx>\n");
  printf("--disable_test <test_idx>   Disable the test <test_idx>\n");
  printf("--list_tests                List all test descriptions\n");
  printf("--num_passes <n>            num of passes to run tests\n");
  printf("--num_iterations <n>        num of iterations in test10 in each pass\n");
  printf("--exit_on_error             exit the prorgam when memory errors are detected\n");
  printf("--random                    Random test. Equivalent to --disable_all --enable_test 7 --exit_on_error --num_passes 1\n");
  printf("--help                      print out this message\n");
  exit(1);
}

int
main(const int argc, const char **argv)
{
  memtest_control_t memtest_ctl[MAX_NUM_DEVICES];
  
  int dev_id = -1;
  int device_count = -1;
  int platform_index = 0;
  bool list_platforms = false;
  bool list_devices = false;
  int i;
  
  if (argc >=2 ){
    if( strcmp(argv[1], "--help")== 0){
      usage(argv);
    }
  }
  
  if(gethostname(hostname, 64) !=0){
    fprintf(stderr, "ERROR: gethostname() returns error\n");
    exit(ERR_GENERAL);
  }
  
  for(i=0;i < 64; i++){
    if (hostname[i] == '.'){
      hostname[i] = 0;
      break;
    }
  }
  
  printf("hostname is %s\n", hostname);

  for (i =1;i < argc; i++){
    
    if( strcmp(argv[i], "--help")== 0){
      usage(argv);
    }
    
	if (strcmp(argv[i], "--enable_test") == 0){
	  if (i+1 >= argc){
		usage(argv);
	  }
	  size_t idx = atoi(argv[i+1]);
	  if (idx >= DIM(cuda_memtests)){
		fprintf(stderr, "Error: invalid test id\n");
		usage(argv);
	  }

	  cuda_memtests[idx].enabled = 1;

	  i++;
	  continue;
	}
	if (strcmp(argv[i], "--disable_test") == 0){
	  if (i+1 >= argc){
		usage(argv);
	  }
	  size_t idx = atoi(argv[i+1]);
	  if (idx >= DIM(cuda_memtests)){
		fprintf(stderr, "Error: invalid test id\n");
		usage(argv);
	  }

	  cuda_memtests[idx].enabled = 0;
	  i++;
	  continue;
	}
	if (strcmp(argv[i], "--disable_all") == 0){
	  size_t k;
	  for (k=0;k < DIM(cuda_memtests);k++){
		cuda_memtests[k].enabled = 0;
	  }
	  continue;
	}
    if( strcmp(argv[i], "--device") == 0){
      if (i+1 >= argc){
        usage(argv);
      }
      dev_id =  atoi(argv[i+1]);
      if (dev_id < 0){
        fprintf(stderr, "Error: invalid device number(%d)\n", dev_id);
        exit(1);
      }
      i++;
      continue;
    }
	if (strcmp(argv[i], "--device_count") == 0){
      if (i+1 >= argc){
		usage(argv);
	  }
	    device_count = atoi(argv[i+1]);
	    i++;
	    continue;
	}
	if (strcmp(argv[i], "--platform") == 0){
      if (i+1 >= argc){
		usage(argv);
	  }
	    platform_index = atoi(argv[i+1]);
	    i++;
	    continue;
	}
    if (strcmp(argv[i], "--list_platforms") == 0){
      list_platforms = true;
      continue;
    }
    if (strcmp(argv[i], "--list_devices") == 0){
      list_devices = true;
      continue;
    }
    if (strcmp(argv[i], "--num_iterations") == 0){
      
      if (i+1 >= argc){
	usage(argv);
      }
      num_iterations = atoi(argv[i+1]);
      if (num_iterations <= 0){
	printf("ERROR: invalid number of iterations\n");
	usage(argv);
      }
      i++;
      continue;
    }
    
    if (strcmp(argv[i], "--num_passes") == 0){
      
      if (i+1 >= argc){
	usage(argv);
      }
      num_passes = atoi(argv[i+1]);
      if (num_passes <= 0){
	printf("ERROR: invalid number of passes\n");
	usage(argv);
      }
      i++;
      continue;
    }


    if (strcmp(argv[i], "--exit_on_error") == 0){
      exit_on_error = 1;
      continue;
    }
    
	if (strcmp(argv[i], "--list_tests") == 0){
	    list_tests_info();
	    return 0;
	}

	if (strcmp(argv[i], "--random") == 0){
	    //equal to "--disable_all --enable_test 7 --exit_on_error --num_passes 1"
	    size_t k;
	    for (k=0;k < DIM(cuda_memtests);k++){
		cuda_memtests[k].enabled = 0;
	    }
	    cuda_memtests[7].enabled = 1;
	    exit_on_error = 1;
	    num_passes = 1;
	    continue;
	}

    printf("ERROR: invalid argument (%s)\n", argv[i]);
    usage(argv);
  }


  if(list_platforms){
    cl_int rc = oclGetPlatformID (-1, NULL);CLERR;
    exit(0);
  }

  cl_platform_id clSelectedPlatformID = NULL;
  cl_int rc = oclGetPlatformID (platform_index, &clSelectedPlatformID);CLERR;
  
  char strbuf[128];
  rc = clGetPlatformInfo (clSelectedPlatformID, CL_PLATFORM_NAME, sizeof(strbuf), strbuf, NULL); CLERR;
  printf("CL_PLATFORM_NAME: \t%s\n", strbuf);
  
  rc = clGetPlatformInfo (clSelectedPlatformID, CL_PLATFORM_VERSION, sizeof(strbuf), strbuf, NULL); CLERR;
  printf("CL_PLATFORM_VERSION: \t%s\n", strbuf);
  
  // Find out how many devices there are
  cl_uint device_count_system;
  rc  = clGetDeviceIDs (clSelectedPlatformID, CL_DEVICE_TYPE_GPU, 0, NULL, &device_count_system); CLERR;
  if (device_count_system == 0){
    printf("ERROR: No openCL device found!\n");
    exit(1);
  }
       
  cl_device_id* devices = (cl_device_id*) malloc(sizeof(cl_device_id) * device_count_system);
  rc = clGetDeviceIDs (clSelectedPlatformID, CL_DEVICE_TYPE_GPU, device_count_system, devices, &device_count_system); CLERR;
  
  if (device_count_system <= 0){
    printf("ERROR: no device found\n");
    exit(1);
  }
  if (dev_id >= ((int)device_count_system)){
    printf("ERROR: invalid device id (%d)\n", dev_id);
    exit(1);
  }

  if (dev_id == -1){
    dev_id = 0;
    device_count = device_count_system;
  } else if (device_count == -1){
    device_count = 1;
  }
  if (device_count + dev_id > device_count_system){
    device_count = device_count_system - dev_id;
    printf("device_count adjusted\n"); 
  }
  printf("device_count=%d\n", device_count);
  if(device_count > MAX_NUM_DEVICES){
	fprintf(stderr, "Error: max number of devices (%d) exceeded: %d\n", MAX_NUM_DEVICES, device_count);
    exit(-1);
  }

  int starti = dev_id;
  int endi = dev_id + device_count;
    
  for(i=starti; i < endi; ++i ){
    clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(strbuf), &strbuf, NULL);CLERR;
    cl_device_type device_type;
    clGetDeviceInfo(devices[i], CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, NULL);
    printf("                  \tDevice %d is %s, \"%s\"\n", i, device_type_str(device_type), strbuf);
  }
  if(list_devices){
    exit(0);
  }  

  cl_context contexts[MAX_NUM_DEVICES];
  for(i=starti;i < endi;i++){
    contexts[i]= clCreateContext(0, 1, &devices[i], NULL, NULL, NULL);
    if (contexts[i] == (cl_context)0){
      printf("ERROR: creating context from GPU failed\n");
      exit(1);
    }
  }
  
  
  cl_command_queue queues[MAX_NUM_DEVICES];
  for(i=starti;i < endi;i++){
    queues[i] = clCreateCommandQueue(contexts[i], devices[i], CL_QUEUE_PROFILING_ENABLE,NULL);
    if (queues[i] == (cl_command_queue)0){
      printf("creating command queue failed\n");
      exit(1);
    }
  }
  

  read_kernel_source();
  
  cl_program programs[MAX_NUM_DEVICES];
  cl_kernel kernels[MAX_NUM_DEVICES][MAX_NUM_KERNELS];
  for(i=starti;i < endi;i++){
    programs[i] = clCreateProgramWithSource(contexts[i], 1, (const char**)&kernel_source, NULL, NULL);
    if (programs[i] == (cl_program)0){
      printf("ERROR: creating program from source failed\n");
      exit(1);
    }
    
    rc = clBuildProgram(programs[i], 0, NULL, NULL, NULL, NULL); CLERR;
  }
    
  
  for(i=starti;i < endi;i++){
    memtest_ctl[i].context = contexts[i];
    memtest_ctl[i].device_idx = i;
    memtest_ctl[i].device = devices[i];
    memtest_ctl[i].queue = queues[i];
    allocate_device_mem(&memtest_ctl[i]);
    memtest_ctl[i].program= programs[i];
  }
  
  
  int j;

  /*
  int loop_count =0;
  while(loop_count < 2){
    for(i=0;i < device_count_system; i++){
      memtest_control_t * mc = &memtest_ctl[i];
      printf("Running tests  in device %d\n", i);
      test10(mc);
    }//i
    
    for(i=0;i < device_count_system; i ++){
      memtest_control_t * mc = &memtest_ctl[i];
      clFinish(mc->queue);
    }
    loop_count++;
  }
  */
  struct timeval t0, t1;
  gettimeofday(&t0, NULL);
  
  pthread_t pid[MAX_NUM_DEVICES];
  for(i=starti;i < endi;i++){
    int rc;
    rc = pthread_create(&pid[i], NULL, run_tests, (void*)&memtest_ctl[i]);
    if (rc != 0){
      printf("ERROR: create pthread in host failed \n");
      exit(1);
    }
  }
  
  for(i=starti;i < endi;i++){
    pthread_join(pid[i], NULL);
  }

  gettimeofday(&t1, NULL);

  for(i=starti;i < endi;i++){
    memtest_control_t* mc = &memtest_ctl[i];
    clReleaseMemObject(mc->device_mem);
    clReleaseMemObject(mc->err_count);
    clReleaseMemObject(mc->err_addr);
    clReleaseMemObject(mc->err_expect);
    clReleaseMemObject(mc->err_current);
    clReleaseMemObject(mc->err_second_read);
    clFinish(queues[i]);
    clReleaseCommandQueue(queues[i]);
    clReleaseContext(contexts[i]);
  }
  
  free(devices);

  printf("OpenCL memtest: %d GPU(s) run %.1f seconds with %d passes\n", device_count, TDIFF(t1, t0), num_passes);

}

