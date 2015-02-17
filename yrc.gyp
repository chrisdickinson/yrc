{
  'target_defaults': {
    'conditions': [
      ['OS != "win"', {
        'defines': [
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
        ]
      }],
    ],
    'xcode_settings': {
        'conditions': [
          [ 'clang==1', {
            'WARNING_CFLAGS': [
              '-Wall',
              '-Wextra',
              '-Wno-unused-parameter',
              '-Wno-dollar-in-identifier-extension'
            ]}, {
           'WARNING_CFLAGS': [
             '-Wall',
             '-Wextra',
             '-Wno-unused-parameter'
          ]}
        ]
      ],
      'OTHER_LDFLAGS': [
      ],
      'OTHER_CFLAGS': [
        '-g',
        '--std=gnu89',
        '-pedantic'
      ],
    }
  },

  'targets': [
    {
      'target_name': 'yrc',
      'type': '<(yrc_library)',
      'include_dirs': [
        'include',
        'src/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
        'conditions': [
          ['OS != "win"', {
            'defines': [
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
            ],
          }],
          ['OS == "mac"', {
            'defines': [ '_DARWIN_USE_64_BIT_INODE=1' ],
          }],
          ['OS == "linux"', {
            'defines': [ '_POSIX_C_SOURCE=200112' ],
          }],
        ],
      },
      'sources': [
        'common.gypi',
        'include/yrc.h',
        'src/accumulator.c',
        'src/llist.c',
        'src/tokenizer.c',
        'src/parser.c',
        'src/pool.c',
        'src/traverse.c',
        'src/str.c',
      ]
    },

    {
      'target_name': 'run-tests',
      'type': 'executable',
      'dependencies': [ 'yrc' ],
      'sources': [
        'test/main.c',
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    }
  ]
}

