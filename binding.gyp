{
  "targets": [
    {
      "target_name": "sack_vfs",
      
      "sources": [ "vfs_module.cc",
           "src/sack.cc",
           "src/sqlite3.c",
           "com_interface.cc",
           "sql_module.cc",
           "thread_module.cc",
          ],
	'defines': [
          'TARGETNAME="sack_vfs.node"',
           "__STATIC__","USE_SQLITE","USE_SQLITE_INTERFACE","FORCE_COLOR_MACROS","NO_OPEN_MACRO"
        ],
    'conditions': [
          ['OS=="linux"', {
            'defines': [
              '__LINUX__',
            ],
            'cflags_cc': ['-Wno-address', '-Wno-strict-aliasing', '-Wno-switch ],
            'include_dirs': [
              'include/linux',
            ],
            'libraries':[ '-luuid' ]
          }],
          ['OS=="mac"', {
            'defines': [
              '__LINUX__','__MAC__'
            ],
            'include_dirs': [
              'include/linux',
            ],
          }],
          ['OS=="win"', {
            'defines': [
              "NEED_SHLAPI","NEED_SHLOBJ","_CRT_SECURE_NO_WARNINGS"
            ],
            'sources': [
              # windows-only; exclude on other platforms.
              'reg_access.cc',
            ],
  	        'libraries':[ 'winmm', 'ws2_32', 'iphlpapi', 'rpcrt4', 'odbc32' ]
          }, { # OS != "win",
            'defines': [
              '__LINUX__',
            ],
          }]
        ],

	'otherDefinss': [ '__64__=1',
		"__NO_OPTIONS__","__NO_INTERFACE_SUPPORT__","__NO_ODBC__" ],

    }
  ],

  "target_defaults": {
  	'include_dirs': ['src',    "<!(node -e \"require('nan')\")" ]
  }
  
}

