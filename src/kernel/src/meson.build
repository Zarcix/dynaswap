sources = files(
    'main.c',
    'chunk.c',
    'swap.c',
    'include/log.c',
)

uuid_dep = dependency('uuid')

executable('dynaswap', sources, dependencies: [uuid_dep])