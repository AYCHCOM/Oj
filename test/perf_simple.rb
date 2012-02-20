#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

#require 'test/unit'
require 'optparse'
require 'yajl'
require 'json'
require 'json/pure'
require 'json/ext'
require 'oj'

$indent = 2

opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

iter = 100000
s = %{
{ "class": "Foo::Bar",
  "attr1": [ true, [false, [12345, null], 3.967, ["something", false], null]],
  "attr2": { "one": 1 }
}
}
#s = File.read('sample.json')

Oj.default_options = { :indent => 0 }

puts

parse_results = { :oj => 0.0, :yajl => 0.0, :pure => 0.0, :ext => 0.0 }

start = Time.now
iter.times do
  Oj.load(s)
end
dt = Time.now - start
parse_results[:oj] = dt
puts "%d Oj.load()s in %0.3f seconds or %0.1f loads/msec" % [iter, dt, iter/dt/1000.0]

start = Time.now
iter.times do
  Yajl::Parser.parse(s)
end
dt = Time.now - start
base = dt
parse_results[:yajl] = dt
puts "%d Yajl::Parser.parse()s in %0.3f seconds or %0.1f parses/msec" % [iter, dt, iter/dt/1000.0]

begin
  JSON.parser = JSON::Ext::Parser
  start = Time.now
  iter.times do
    JSON.parse(s)
  end
  dt = Time.now - start
  base = dt
  base_name = 'JSON::Ext'
  parse_results[:ext] = dt
  puts "%d JSON::Ext::Parser parse()s in %0.3f seconds or %0.1f parses/msec" % [iter, dt, iter/dt/1000.0]
rescue Exception => e
  puts "JSON::Ext failed: #{e.class}: #{e.message}"
end

begin
  JSON.parser = JSON::Pure::Parser
  start = Time.now
  iter.times do
    JSON.parse(s)
  end
  dt = Time.now - start
  base = dt
  base_name = 'JSON::Pure'
  parse_results[:pure] = dt
  puts "%d JSON::Pure::Parser parse()s in %0.3f seconds or %0.1f parses/msec" % [iter, dt, iter/dt/1000.0]
rescue Exception => e
  puts "JSON::Pure failed: #{e.class}: #{e.message}"
end

puts "Parser results:"
puts "gem    seconds  parses/msec  X faster than #{base_name}"
parse_results.each do |name,dt|
  puts "%-4s  %6.3f    %5.1f        %4.1f" % [name, dt, iter/dt/1000.0, base/dt]
end

puts

dump_results = { :oj => 0.0, :yajl => 0.0, :pure => 0.0, :ext => 0.0 }
obj = Oj.load(s)

start = Time.now
iter.times do
  Oj.dump(obj)
end
dt = Time.now - start
parse_results[:oj] = dt
puts "%d Oj.dump()s in %0.3f seconds or %0.1f dumps/msec" % [iter, dt, iter/dt/1000.0]

start = Time.now
iter.times do
  Yajl::Encoder.encode(obj)
end
dt = Time.now - start
base = dt
base_name = 'YAJL'
parse_results[:yajl] = dt
puts "%d Yajl::Encoder.encode()s in %0.3f seconds or %0.1f encodes/msec" % [iter, dt, iter/dt/1000.0]

begin
  JSON.parser = JSON::Ext::Parser
  start = Time.now
  iter.times do
    JSON.generate(obj)
  end
  dt = Time.now - start
  base = dt
  base_name = 'JSON::Ext'
  parse_results[:pure] = dt
  puts "%d JSON::Ext generate()s in %0.3f seconds or %0.1f generates/msec" % [iter, dt, iter/dt/1000.0]
rescue Exception => e
  parse_results[:ext] = 0.0
  puts "JSON::Ext failed: #{e.class}: #{e.message}"
end

begin
  JSON.parser = JSON::Pure::Parser
  start = Time.now
  iter.times do
    JSON.generate(obj)
  end
  dt = Time.now - start
  base = dt
  base_name = 'JSON::Pure'
  parse_results[:pure] = dt
  puts "%d JSON::Pure generate()s in %0.3f seconds or %0.1f generates/msec" % [iter, dt, iter/dt/1000.0]
rescue Exception => e
  parse_results[:pure] = 0.0
  puts "JSON::Pure failed: #{e.class}: #{e.message}"
end

puts "Parser results:"
puts "gem    seconds  dumps/msec  X faster than #{base_name}"
parse_results.each do |name,dt|
  if 0.0 == dt
    puts "#{name} failed to generate JSON"
    next
  end
  puts "%-4s  %6.3f    %5.1f       %4.1f" % [name, dt, iter/dt/1000.0, base/dt]
end

puts
