#include <kernel.h>

#include "template.h"


bool module_init() {
	// Module init function

	return true;	// Return true on success. False will abort.
}

module_name("Template Module")
module_version(1,2,123);
module_author("Shmorry Shmippolito");
module_description("The description of this module");
module_oninitialize(module_init);
// Any other module directives go here (see doc)
