#include <memory>
#include <fstream>
#include <cerrno>

#include "common.hpp"
#include "log.hpp"
#include "partial_flow_graph.hpp"
#include "instruction.hpp"

using namespace std;

/**
 *  GetLayer returns the type 
 * of priority that the plugin has
 */
size_t GetLayer()
{
	// the most highest priority
	return PLUGIN_LAYER;
}

/**
 * engine_share_buff start of shared memory
 * from the engine with the plugin
 */
BYTE* engine_share_buff; 

/**
 * file_mapping handler address to a shared
 * file location with the engine
 */
static HANDLE file_mapping;

/**
 * pfg holds the current partial flow graph nodes and structures
 */
static unique_ptr<PartialFlowGraph> pfg;


BOOL DBTInit()
{
	file_mapping = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, memsharedname);
	if (!file_mapping)
		return FALSE;

    engine_share_buff = (BYTE*) MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE);
    if (!engine_share_buff)
        return FALSE;

	const char *logname = LOGNAME_BUFFER(engine_share_buff);
	string name = (!logname) ? string() : string(logname);
    
	auto* file = new fstream(name, ios::app);
	if (!(*file))
		return FALSE;

	log_init(file);
	
	log_info("[DBTITrace] Init is called");
	pfg = make_unique<PartialFlowGraph>();

	return TRUE;
}

PluginReport* DBTBranching(void* custom_params, PluginLayer** layers)
{
    return (PluginReport*) 
        VirtualAlloc(0, sizeof(PluginReport), MEM_COMMIT, PAGE_READWRITE);
}

PluginReport* DBTBeforeExecute(void *custom_params, PluginLayer **layers)
{
	CUSTOM_PARAMS* params = (CUSTOM_PARAMS *)custom_params;

	size_t eip = params->MyDisasm->EIP;
	Int32 branch_type = params->MyDisasm->Instruction.BranchType;
	size_t value = (size_t)params->MyDisasm->Instruction.AddrValue;
	char* complete_instruction = params->MyDisasm->CompleteInstr;

	char instr_bytes[100] = { 0 };
    char temp[MAX_PATH];
	size_t len = params->instrlen;
    
	PluginReport* report = (PluginReport*)VirtualAlloc(0, sizeof(PluginReport), MEM_COMMIT, PAGE_READWRITE);
	if (!report)
		return nullptr;

    char* content = (char*)VirtualAlloc(0, 0x4000, MEM_COMMIT, PAGE_READWRITE);
	if (!content)
		return report;

    if (params->stack_trace)
        StackTrace((ExecutionContext*)params->tdata->PrivateStack, content);
    else
        memset(content, 0, 0x4000);

#ifndef _M_X64
    sprintf(temp, "%08X: ", (int)params->MyDisasm->VirtualAddr);
#else
    sprintf(temp, "%08X%08X: ", (DWORD)(MyDisasm->VirtualAddr >> 32), (DWORD)(MyDisasm->VirtualAddr & 0xFFFFFFFF));
#endif

    strcat(content, temp);
	for (size_t i = 0; i < len; i++)
        sprintf(instr_bytes + i * 3, "%02X ", *(BYTE*) (eip + i));

    sprintf(temp, "%-*s : %s", 45, instr_bytes, complete_instruction);
    strcat(content, temp);

	auto instruction = Instruction(eip, complete_instruction, branch_type, len, value);

	pfg->add(instruction);
    
	// pack the plugin response
    report->plugin_name = "DBTTrace";
    report->content_before = content;
    report->content_after = 0;

    return report;
}

PluginReport* DBTAfterExecute(void* custom_params, PluginLayer** layers)
{
    return (PluginReport*) 
		VirtualAlloc(0, sizeof(PluginReport), MEM_COMMIT, PAGE_READWRITE);
}

PluginReport* DBTFinish()
{
	log_info("[DBTITrace] Finish is called");
	
	auto plugin = (PluginReport*)
		VirtualAlloc(0, sizeof(PluginReport), MEM_COMMIT, PAGE_READWRITE);
	
	if (!plugin) {
		log_error("Finish plugin virtual allocation failed");
		return plugin;
	}

	fstream out("partiaflowgraph.dot", fstream::out);
	if (!out) {
		log_error("cannot open partialflowgraph.dot : %s", strerror(errno));
		return plugin;
	}

	auto graphviz = pfg->graphviz();
	int err = pfg->generate(graphviz, &out);
	if (err != 0) {
		log_error("cannot generate partial flow graph");
		return plugin;
	}

	BYTE *cfg_shared_mem = CFG(engine_share_buff);
	auto from = PartialFlowGraph();

	size_t size = cfg_buf_size();
	err = from.deserialize(cfg_shared_mem, size);
	if (err != 0) {
		log_error("cannot deserialize from shard buffer engine a pfg");
		return plugin;
	}

	err = pfg->merge(from);
	if (err != 0) {
		log_error("cannot merge two partial flow graphs");
		return plugin;
	}

	err = pfg->serialize(cfg_shared_mem, size);
	if (err != 0) {
		log_error("cannot serialize from pfg to shared buffer engine");
		return plugin;
	}
	
	CloseHandle(file_mapping);

	return plugin;
}