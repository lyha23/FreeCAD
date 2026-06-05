#include "cad_core/assembly/assembly_link.h"

#include "cad_core/app/link_support.h"

namespace cad_core::assembly {

void executeAssemblyLink(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyLink.cpp
    // ::AssemblyLink::execute(), calls "updateContents()" and then "App::Part::execute()";
    // LinkedObject is the sub-assembly or component link target, Rigid remains metadata here.
    app::executeAppLinkBaseLike(object,
                                context,
                                {"LinkedObject", "Rigid", "Group", "Type", "Id", "Uid"},
                                "assembly_link");
}

}  // namespace cad_core::assembly
