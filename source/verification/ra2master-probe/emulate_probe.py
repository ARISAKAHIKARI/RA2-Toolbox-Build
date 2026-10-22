# Model-only fallback for uncalled, non-core imported modules missing from catalog.
# No host Windows API is invoked. Unsupported behavior is not game evidence.
import sys, pefile
pe=pefile.PE(sys.argv[1])
imports={}
for d in pe.DIRECTORY_ENTRY_IMPORT:
    imports[d.dll.decode().lower()]=[{"name":x.name.decode() if x.name else None,"ordinal":x.ordinal if x.name is None else n+1} for n,x in enumerate(d.imports)]
from unpack.catalog import Catalog
orig = Catalog.exports
missing=set()
def exports(self,name):
    try:return orig(self,name)
    except ValueError as e:
        if not str(e).startswith('module missing from catalog '):raise
        if name not in missing:print('SYNTHETIC EXPORT CATALOG:',name,flush=True);missing.add(name)
        return imports.get(name.lower(),[])
Catalog.exports=exports
from unpack.api_handlers import ApiHandlers
def check_remote(self,symbol,process,out,a,b):
    if not self.writable_range(out,4):return 0,{"model":"invalid output"}
    self.uc.mem_write(out,bytes(4))
    return 1,{"model":"no debugger in synthetic process"}
ApiHandlers._handlers["checkremotedebuggerpresent"]=(check_remote,2)
from unpack.tracer import main
main()
