import readmscan

model = readmscan.model_tests(file('model.out'))
mscan = readmscan.mscan(file('mscan-xv6.out'))
mscan = mscan.join('', model, '', 'calls', 'path', 'testnum')

calls = [
    # Things that deal with names
    'open', 'link', 'unlink', 'rename', 'stat',
    # Things that deal with FDs
    'fstat', 'lseek', 'close', 'read', 'write', 'pread', 'pwrite',
    # VM
    'mmap', 'munmap', 'mprotect', 'memread', 'memwrite']

print 'Shared cases (%d):' % mscan.shared
print mscan.table_ul(calls).mapget('shared').text(shade=True)
print

nonidem = mscan.filter(lambda idempotent_projs: idempotent_projs == [[],[]])
print 'Non-idempotent/unknown idempotence shared cases (%d):' % nonidem.shared
print nonidem.table_ul(calls).mapget('shared').text(shade=True)
print



print 'Non-idempotent shared case list:'
print mscan.filter(lambda shared, idempotent_projs, idempotence_unknown:
                   shared and idempotent_projs == [[],[]] and
                   idempotence_unknown == 0).str_table()
print

print 'Unknown idempotence shared case list:'
print mscan.filter(lambda shared, idempotent_projs, idempotence_unknown:
                   shared and idempotent_projs == [[],[]] and
                   idempotence_unknown != 0).str_table()
print

print 'Idempotent shared case list:'
print mscan.filter(lambda shared, idempotent_projs:
                   shared and idempotent_projs != [[],[]]).str_table()
print



# print 'Idempotent conflict-free case list:'
# print mscan.filter(lambda shared, idempotent_projs:
#                    (not shared) and idempotent_projs != [[],[]]).str_table()
# print



# print (mscan.filter(lambda idempotent_projs, shared:
#                     shared != [] and idempotent_projs == [[],[]]).
# #    filter(lambda calls: calls==('open','pread')).
#        str_table())
# print

# print 'Idempotent non-shared cases:'
# print mscan.filter(lambda idempotent_projs: idempotent_projs != [[],[]]).table_ul(calls).mapget('nonshared').text(shade=True)
# print

# print (mscan.filter(lambda idempotent_projs, shared:
#                     shared == [] and idempotent_projs != [[],[]]).
# #    filter(lambda calls: calls==('open','pread')).
#        str_table())
# print
