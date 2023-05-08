project('mmxmpcr', 'cpp',
        version : '2004-02-18',
        default_options : ['warning_level=everything', 'cpp_std=c++98'])

motif = meson.get_compiler('cpp').find_library('Xm', required : true)
xt = dependency('xt', required: true)

executable('mmxmpcr',
           'mmxmpcr.cpp',
           cpp_args : '-DMMXMPCR_DEVICE="' + get_option('mmxmpcr_device') + '"',
           dependencies : [motif, xt],
           install : true)
