SymInode    = tstruct(data  = tlist(SymByte),
                      nlink = SymInt)
SymIMap     = tdict(SymInt, SymInode)
SymFilename = tuninterpreted('Filename')
SymDir      = tdict(SymFilename, SymInt)

def __init__(self, ...):
  self.fname_to_inum = SymDir.any()
  self.inodes = SymIMap.any()

@symargs(src=SymFilename, dst=SymFilename)
def rename(self, src, dst):
  if not self.fname_to_inum.contains(src):
    return (-1, errno.ENOENT)
  if src == dst:
    return 0
  if self.fname_to_inum.contains(dst):
    self.inodes[self.fname_to_inum[dst]].nlink -= 1
  self.fname_to_inum[dst] = self.fname_to_inum[src]
  del self.fname_to_inum[src]
  return 0
