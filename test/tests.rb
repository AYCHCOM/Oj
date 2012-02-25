#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'test/unit'
require 'oj'

class Jam
  attr_reader :x, :y

  def initialize(x, y)
    @x = x
    @y = y
  end

  def eql?(o)
    self.class == o.class && @x == o.x && @y == o.y
  end
  alias == eql?

end # Jam

class Jeez < Jam
  def initialize(x, y)
    super
  end
  
  def to_json()
    %{{"x":#{@x},"y":#{@y}}}
  end
  
end # Jeez

class Jazz < Jam
  def initialize(x, y)
    super
  end
  def to_hash()
    { 'x' => @x, 'y' => @y }
  end
end # Jazz

class Juice < ::Test::Unit::TestCase

  def test_get_options
    opts = Oj.default_options()
    assert_equal(opts, {
                   :encoding=>nil,
                   :indent=>0,
                   :circular=>false,
                   :mode=>:object})
  end

  def test_set_options
    orig = {
      :encoding=>nil,
      :indent=>0,
      :circular=>false,
      :mode=>:object}
    o2 = {
      :encoding=>"UTF-8",
      :indent=>4,
      :circular=>true,
      :mode=>:compat}
    o3 = { :indent => 4 }
    Oj.default_options = o2
    opts = Oj.default_options()
    assert_equal(opts, o2);
    Oj.default_options = o3 # see if it throws an exception
    Oj.default_options = orig # return to original
  end

  def test_nil
    dump_and_load(nil, false)
  end

  def test_true
    dump_and_load(true, false)
  end

  def test_false
    dump_and_load(false, false)
  end

  def test_fixnum
    dump_and_load(0, false)
    dump_and_load(12345, false)
    dump_and_load(-54321, false)
    dump_and_load(1, false)
  end

  def test_bignum
    dump_and_load(12345678901234567890123456789, false)
  end

  def test_float
    dump_and_load(0.0, false)
    dump_and_load(12345.6789, false)
    dump_and_load(-54321.012, false)
    dump_and_load(2.48e16, false)
  end

  def test_string
    dump_and_load('', false)
    dump_and_load('abc', false)
    dump_and_load("abc\ndef", false)
    dump_and_load("a\u0041", false)
  end

  def test_encode
    Oj.default_options = { :encoding => 'UTF-8' }
    dump_and_load("ぴーたー", false)
    Oj.default_options = { :encoding => nil }
  end

  def test_array
    dump_and_load([], false)
    dump_and_load([true, false], false)
    dump_and_load(['a', 1, nil], false)
    dump_and_load([[nil]], false)
    dump_and_load([[nil], 58], false)
  end

  # Symbol
  def test_symbol_strict
    begin
      json = Oj.dump(:abc, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_symbol_null
    json = Oj.dump(:abc, :mode => :null)
    assert_equal('null', json)
  end
  def test_symbol_compat
    json = Oj.dump(:abc, :mode => :compat)
    assert_equal('"abc"', json)
  end    
  def test_symbol_object
    Oj.default_options = { :mode => :object }
    dump_and_load(''.to_sym, false)
    dump_and_load(:abc, false)
    dump_and_load(':xyz'.to_sym, false)
  end

  # Time
  def test_time_strict
    t = Time.new(2012, 1, 5, 23, 58, 7)
    begin
      json = Oj.dump(t, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_time_null
    t = Time.new(2012, 1, 5, 23, 58, 7)
    json = Oj.dump(t, :mode => :null)
    assert_equal('null', json)
  end
  def test_time_compat
    t = Time.new(2012, 1, 5, 23, 58, 7)
    json = Oj.dump(t, :mode => :compat)
    assert_equal(%{1325775487.000000}, json)
  end    
  def test_time_object
    t = Time.now()
    Oj.default_options = { :mode => :object }
    dump_and_load(t, false)
  end

# Class
  def test_class_strict
    begin
      json = Oj.dump(Juice, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_class_null
    json = Oj.dump(Juice, :mode => :null)
    assert_equal('null', json)
  end
  def test_class_compat
    json = Oj.dump(Juice, :mode => :compat)
    assert_equal(%{"Juice"}, json)
  end    
  def test_class_object
    Oj.default_options = { :mode => :object }
    dump_and_load(Juice, false)
  end

# Hash
  def test_hash
    Oj.default_options = { :mode => :strict }
    dump_and_load({}, false)
    dump_and_load({ 'true' => true, 'false' => false}, false)
    dump_and_load({ 'true' => true, 'array' => [], 'hash' => { }}, false)
  end
  def test_non_str_hash_strict
    begin
      json = Oj.dump({ 1 => true, 0 => false }, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_non_str_hash_null
    begin
      json = Oj.dump({ 1 => true, 0 => false }, :mode => :null)
    rescue Exception => e
      assert(true)
    end
  end
  def test_non_str_hash_compat
    begin
      json = Oj.dump({ 1 => true, 0 => false }, :mode => :compat)
    rescue Exception => e
      assert(true)
    end
  end
  def test_non_str_hash_object
    Oj.default_options = { :mode => :object }
    json = Oj.dump({ 1 => true, 0 => false, :sim => nil })
    h = Oj.load(json, :mode => :strict)
    assert_equal({"^#1" => [1, true], "^#2" => [0, false], ":sim" => nil}, h)
    h = Oj.load(json)
    assert_equal({ 1 => true, 0 => false, :sim => nil }, h)
  end
  def test_mixed_hash_object
    Oj.default_options = { :mode => :object }
    json = Oj.dump({ 1 => true, 0 => false, 'nil' => nil, :sim => 4 })
    h = Oj.load(json, :mode => :strict)
    assert_equal({"^#1" => [1, true], "^#2" => [0, false], "nil" => nil, ":sim" => 4}, h)
    h = Oj.load(json)
    assert_equal({ 1 => true, 0 => false, 'nil' => nil, :sim => 4 }, h)
  end

# Object with to_json()
  def test_json_object_strict
    obj = Jeez.new(true, 58)
    begin
      json = Oj.dump(obj, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_json_object_null
    obj = Jeez.new(true, 58)
    json = Oj.dump(obj, :mode => :null)
    assert_equal('null', json)
  end
  def test_json_object_compat
    obj = Jeez.new(true, 58)
    json = Oj.dump(obj, :mode => :compat, :indent => 2)
    assert_equal(%{{"x":true,"y":58}}, json)
  end
  def test_json_object_object
    obj = Jeez.new(true, 58)
    json = Oj.dump(obj, :mode => :object, :indent => 2)
    assert_equal(%{{
  "^o":"Jeez",
  "x":true,
  "y":58}}, json)
    obj2 = Oj.load(json, :mode => :object)
    assert_equal(obj, obj2)
  end

# Object with to_hash()
  def test_to_hash_object_strict
    obj = Jazz.new(true, 58)
    begin
      json = Oj.dump(obj, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_to_hash_object_null
    obj = Jazz.new(true, 58)
    json = Oj.dump(obj, :mode => :null)
    assert_equal('null', json)
  end
  def test_to_hash_object_compat
    obj = Jazz.new(true, 58)
    json = Oj.dump(obj, :mode => :compat, :indent => 2)
    assert_equal(%{{
  "x":true,
  "y":58}}, json)
  end
  def test_to_hash_object_object
    obj = Jazz.new(true, 58)
    json = Oj.dump(obj, :mode => :object, :indent => 2)
    assert_equal(%{{
  "^o":"Jazz",
  "x":true,
  "y":58}}, json)
    obj2 = Oj.load(json, :mode => :object)
    assert_equal(obj, obj2)
  end

# Object without to_json() or to_hash()
  def test_object_strict
    obj = Jam.new(true, 58)
    begin
      json = Oj.dump(obj, :mode => :strict)
    rescue Exception => e
      assert(true)
    end
  end
  def test_object_null
    obj = Jam.new(true, 58)
    json = Oj.dump(obj, :mode => :null)
    assert_equal('null', json)
  end
  def test_object_compat
    obj = Jam.new(true, 58)
    json = Oj.dump(obj, :mode => :compat, :indent => 2)
    assert_equal(%{{
  "x":true,
  "y":58}}, json)
  end
  def test_object_object
    obj = Jam.new(true, 58)
    json = Oj.dump(obj, :mode => :object, :indent => 2)
    assert_equal(%{{
  "^o":"Jam",
  "x":true,
  "y":58}}, json)
    obj2 = Oj.load(json, :mode => :object)
    assert_equal(obj, obj2)
  end

  def test_exception
    err = nil
    begin
      raise StandardError.new('A Message')
    rescue Exception => e
      err = e
    end
    json = Oj.dump(err, :mode => :object, :indent => 2)
    #puts "*** #{json}"
    e2 = Oj.load(json, :mode => :strict)
    assert_equal(err.class.to_s, e2['^o'])
    assert_equal(err.message, e2['~mesg'])
    assert_equal(err.backtrace, e2['~bt'])
    e2 = Oj.load(json, :mode => :object)
    assert_equal(e, e2);
  end

  def dump_and_load(obj, trace=false)
    json = Oj.dump(obj, :indent => 2)
    puts json if trace
    loaded = Oj.load(json);
    assert_equal(obj, loaded)
    loaded
  end

end
