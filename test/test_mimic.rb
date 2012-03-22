#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'test/unit'
require 'stringio'
require 'oj'

class Mimic < ::Test::Unit::TestCase

  def test0_mimic_json
    assert(defined?(JSON).nil?)
    Oj.mimic_JSON
    assert(!defined?(JSON).nil?)
  end

# dump
  def test_dump_string
    json = JSON.dump([1, true, nil])
    assert_equal(%{[1,true,null]}, json)
  end

  def test_dump_io
    s = StringIO.new()
    json = JSON.dump([1, true, nil], s)
    assert_equal(s, json)
    assert_equal(%{[1,true,null]}, s.string)
  end
  # TBD options

# load
  def test_load_string
    json = %{{"a":1,"b":[true,false]}}
    obj = JSON.load(json)
    assert_equal({ 'a' => 1, 'b' => [true, false]}, obj)
  end

  def test_load_io
    json = %{{"a":1,"b":[true,false]}}
    obj = JSON.load(StringIO.new(json))
    assert_equal({ 'a' => 1, 'b' => [true, false]}, obj)
  end

  def test_load_proc
    children = []
    json = %{{"a":1,"b":[true,false]}}
    p = Proc.new {|x| children << x }
    obj = JSON.load(json, p)
    assert_equal({ 'a' => 1, 'b' => [true, false]}, obj)
    assert_equal([1, true, false, [true, false], { 'a' => 1, 'b' => [true, false]}], children)
  end

# []


# recurse_proc
  def test_recurse_proc
    children = []
    obj = JSON.recurse_proc({ 'a' => 1, 'b' => [true, false]}) { |x| children << x }
    assert_equal([1, true, false, [true, false], { 'a' => 1, 'b' => [true, false]}], children)
  end

end # Mimic
