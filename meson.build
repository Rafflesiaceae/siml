project('siml', 'c',
  version: '0.1.0',
  default_options: ['c_std=c89', 'warning_level=2']
)

siml_dump = executable('siml-dump', 'siml-dump.c')
siml_roundtrip = executable('siml-roundtrip', 'siml-roundtrip.c')

py = import('python').find_installation()

test(
  'integration',
  py,
  args: files('tests.py'),
  depends: [siml_dump, siml_roundtrip],
)
