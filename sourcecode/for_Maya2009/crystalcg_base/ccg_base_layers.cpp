#include <stdlib.h>
#include <string.h>
#include "shader.h"
#include "geoshader.h"
#include "mayaapi.h"
#include "crystalcg.h"
#include "ccgFunction.h"

#pragma warning(disable : 4267)

struct ccg_base_layers {
	miInteger		n_layers;
	miBoolean		g_cc_enable;
	miBoolean		
};

extern "C" DLLEXPORT void ccg_base_layers_init(      /* init shader */
    miState         *state,
    struct ccg_base_layers *paras,
    miBoolean       *inst_req)
{

}

extern "C" DLLEXPORT void ccg_base_layers_exit(      /* exit shader */
	miState         *state,
	struct ccg_base_layers *paras)
{

}

extern "C" DLLEXPORT int ccg_base_layers_version(void) {return(1);}

extern "C" DLLEXPORT miBoolean ccg_base_layers(
  miColor   *result,
  miState   *state,
  struct ccg_base_layers *paras)
{

  
	return(miTRUE);
}
