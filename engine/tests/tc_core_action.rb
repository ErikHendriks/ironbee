# Test features of the core module which may include directives, actions, etc.
class TestAction < CLIPPTest::TestCase
  include CLIPPTest

  def test_setvar_init_float
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => """
        InitVar A 2.toFloat()
      """,
      :default_site_config => <<-EOS
        Rule A @clipp_print_type "type of A" id:1 rev:1 phase:REQUEST_HEADER
        Rule A @clipp_print      "val of A"  id:2 rev:1 phase:REQUEST_HEADER
      EOS
    )
    assert_log_match /val of A.*2/
    assert_log_match /type of A.*FLOAT/
  end

  def test_setvar_init_int
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => """
        InitVar A 2.toInteger()
      """,
      :default_site_config => <<-EOS
        Rule A @clipp_print_type "type of A" id:1 rev:1 phase:REQUEST_HEADER
        Rule A @clipp_print      "val of A"  id:2 rev:1 phase:REQUEST_HEADER
      EOS
    )
    assert_log_match /val of A.*2/
    assert_log_match /type of A.*NUMBER/
  end

  def test_setvar_sabc
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Action id:site/028 phase:REQUEST_HEADER "setvar:s=abc"
        Rule s @clipp_print "value of s" id:2 rev:1 phase:REQUEST_HEADER
      EOS
    )

    assert_no_issues
    assert_log_match /\[value of s\]: abc/

  end

  def test_setvar_4element_array
    clipp(
      :input_hashes => [simple_hash("GET /foo\n")],
      :default_site_config => <<-EOS
        Rule foo @nop x id:1 rev:1 phase:REQUEST_HEADER "setvar:TestCollection:a=1"
        Rule foo @nop x id:3 rev:1 phase:REQUEST_HEADER "setvar:TestCollection:b=2"
        Rule foo @nop x id:4 rev:1 phase:REQUEST_HEADER "setvar:TestCollection:b=3"
        Rule foo @nop x id:5 rev:1 phase:REQUEST_HEADER "setvar:TestCollection:c=4"

        Rule TestCollection @clipp_print "TestCollection" id:8 rev:1 phase:REQUEST_HEADER
      EOS
    )

    assert_no_issues
    assert_log_match /clipp_print \[TestCollection\]: 1/
    assert_log_match /clipp_print \[TestCollection\]: 3/
    assert_log_match /clipp_print \[TestCollection\]: 4/

  end

  def test_block_advisory_sets_flags
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :modules => ['txdump'],
      :default_site_config => <<-EOS
        Action id:test/1 phase:REQUEST_HEADER "block"
        Rule FLAGS:block @clipp_print "value of block" id:test/3 rev:1 phase:REQUEST_HEADER
      EOS
    )

    assert_no_issues
    assert_log_no_match /\[value of block\]: 0/
    assert_log_match /\[value of block\]: 1/
  end

  def test_setvar_types
    clipp(
      :input_hashes => [simple_hash("GET /foo\n")],
      :default_site_config => <<-EOS

        # RNS-272:
        InitVar fval 234.567
        Rule    fval @clipp_print      "fval" id:qa-site/030 phase:REQUEST_HEADER
        Rule    fval @clipp_print_type "fval" id:qa-site/031 phase:REQUEST_HEADER

        Action id:qa-site/033 phase:REQUEST_HEADER "setvar:ival=%{fval}.toFloat().round()"
        Rule    ival @clipp_print      "ival" id:qa-site/034 phase:REQUEST_HEADER
        Rule    ival @clipp_print_type "ival" id:qa-site/035 phase:REQUEST_HEADER

        Action id:qa-site/039 phase:REQUEST_HEADER "setvar:ival2=%{fval}.toFloat().toInteger()"
        Rule    ival2 @clipp_print      "ival2" id:qa-site/040 phase:REQUEST_HEADER
        Rule    ival2 @clipp_print_type "ival2" id:qa-site/041 phase:REQUEST_HEADER
      EOS
    )

    assert_no_issues
    assert_log_match /clipp_print \[fval\]: 234.567/
    assert_log_match /clipp_print_type \[fval\]: FLOAT/
    assert_log_match /clipp_print \[ival\]: 235/
    assert_log_match /clipp_print_type \[ival\]: NUMBER/
    assert_log_match /clipp_print \[ival2\]: 234/
    assert_log_match /clipp_print_type \[ival2\]: NUMBER/

  end

  def test_setvar_double_increment_bug
    clipp(
        modhtp: true,
        modules: ['pcre'],
        default_site_config: <<-EOS
          Rule ARGS @nop "" id:1a rev:1 phase:REQUEST_HEADER "setvar:RCE:%{FIELD_NAME}+=5"
          Rule ARGS @nop "" id:1b rev:1 phase:REQUEST_HEADER "setvar:RCE:%{FIELD_NAME}+=5"
          Rule RCE:param1 @clipp_print "param1 value" id:2 rev:1 phase:REQUEST_HEADER
          Rule RCE:param1 @clipp_print_type "param1 type" id:3 rev:1 phase:REQUEST_HEADER
          Rule RCE:param2 @clipp_print "param2 value" id:4 rev:1 phase:REQUEST_HEADER
          Rule RCE:param2 @clipp_print_type "param2 type" id:5 rev:1 phase:REQUEST_HEADER
        EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET /test.php?param1=aaabbb&param2=zzzzzzzzzz HTTP/1.0"
        )
      end
    end

    assert_no_issues
    assert_log_match /\[param1 value\]: 10/
    assert_log_match /\[param2 value\]: 10/
    assert_log_match /\[param1 type\]: NUMBER/
    assert_log_match /\[param2 type\]: NUMBER/

    assert_log_no_match /(?:.*\[param1 value\]: 10){2,}/m
    assert_log_no_match /(?:.*\[param2 value\]: 10){2,}/m
    assert_log_no_match /(?:.*\[param1 type\]: NUMBER){2,}/m
    assert_log_no_match /(?:.*\[param2 type\]: NUMBER){2,}/m
  end

  def test_action_redirect_simple
    clipp(
      log_level: 'debug',
      default_site_config: <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER redirect:301,http://www.example.com/?q=ironbee
      EOS
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.0')
        t.response(raw: '200 OK')
      end
    end

    assert_log_match 'clipp_error: 301'
    assert_log_match 'clipp_header: dir=response action=set hdr=Location value=http://www.example.com/?q=ironbee'
  end

  def test_action_redirect_default_status
    clipp(
      log_level: 'debug',
      default_site_config: <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER redirect:http://www.example.com/?q=ironbee,ok
      EOS
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.0')
        t.response(raw: '200 OK')
      end
    end

    assert_log_match 'clipp_error: 302'
    assert_log_match 'clipp_header: dir=response action=set hdr=Location value=http://www.example.com/?q=ironbee,ok'
  end

  def test_action_redirect_use_req_path
    clipp(
      log_level: 'debug',
      modhtp: true,
      default_site_config: <<-EOS
        Action id:1 rev:1 phase:REQUEST redirect:http://www.example.com
      EOS
    ) do
      transaction do |t|
        t.request(raw: 'GET /foo HTTP/1.0')
        t.response(raw: '200 OK')
      end
    end

    assert_log_match 'clipp_error: 302'
    assert_log_match 'clipp_header: dir=response action=set hdr=Location value=http://www.example.com/foo'
  end

end
