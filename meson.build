project('siml', 'c',
  version: '0.1.0',
  default_options: ['c_std=c89', 'warning_level=2']
)

parser = static_library('siml-parser', 'siml.c')
parser_dep = declare_dependency(
  link_with: parser,
  include_directories: include_directories('.'),
)

siml_dump = executable('siml-dump', 'siml-dump.c', dependencies: parser_dep)
siml_roundtrip = executable('siml-roundtrip', 'siml-roundtrip.c', dependencies: parser_dep)

py = import('python').find_installation()

test(
  'integration',
  py,
  args: files('tests/integration/test_basic.py'),
  depends: [siml_dump, siml_roundtrip],
)
