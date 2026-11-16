sources = files(
    'main.c',
    'args.c',
    'chunk.c',
    'swap.c',
    'include/log.c',
    'include/argparse.c',
)

uuid_dep = dependency('uuid')

executable('dynaswap', sources, dependencies: [uuid_dep])