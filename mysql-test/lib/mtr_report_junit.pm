# -*- cperl -*-
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_report_junit;

use strict;
use warnings;
use XML::Simple;
use POSIX qw(strftime);
use base qw(Exporter);

our @EXPORT= qw(mtr_report_stats_junit);

sub mtr_report_stats_junit {
  my $tests    = shift;
  my $filename = shift;
  my $testinfo;
  my $doc;

  foreach my $tinfo (@$tests) {
    my $suite = $tinfo->{name} =~ /^([^\.]+)\./ ? $1 : 'unknown';
    $testinfo->{$suite}{tot_tests}++;
    $testinfo->{$suite}{tot_failed}++  if $tinfo->{failures};
    $testinfo->{$suite}{tot_skipped}++ if $tinfo->{skip};
    $testinfo->{$suite}{tot_passed}++  if $tinfo->{result} eq 'MTR_RES_PASSED';
    push (@{$testinfo->{$suite}{tests}}, $tinfo);
  }

  foreach my $suite (keys %$testinfo) {
    my $suitetime = 0;
    my @testcases;

    foreach my $tinfo (@{$testinfo->{$suite}{tests}}) {
      my $name = $tinfo->{shortname};
      $name .= '_' . $tinfo->{combination} if $tinfo->{combination};

      my $testtime = $tinfo->{timer} ? $tinfo->{timer} / 1000 : 0;
      $suitetime += $testtime;

      my $testcase = gen_testcase ($name, $tinfo->{name}, $testtime);
      if ($tinfo->{failures}) {
	my @lines = split (/\n/, $tinfo->{logfile});
	my $message = $lines[$#lines];
	my $failure = gen_failure (
          $tinfo->{result},
          $message,
          $tinfo->{logfile}
        );
	push @{$testcase->{failure}}, $failure;
      }

      if ($tinfo->{skip}) {
	my $message = $tinfo->{comment} ? $tinfo->{comment} : 'unknown reason';
        # Failures and skips have the same structure
	my $skipped = gen_failure ($tinfo->{result}, $message, $message);
	push @{$testcase->{skipped}}, $skipped;
      }
      push @testcases, $testcase;
    }

    my $tot_failed = $testinfo->{$suite}{tot_failed} ? 
      $testinfo->{$suite}{tot_failed} : 0;

    my $tot_skipped = $testinfo->{$suite}{tot_skipped} ? 
      $testinfo->{$suite}{tot_skipped} : 0;

    my $testsuite = gen_testsuite (
      $suite, 
      $suitetime, 
      $tot_failed, 
      $tot_skipped,
      $testinfo->{$suite}{tot_tests}
    );
    push @{$testsuite->{testcase}}, @testcases;
    push @{$doc->{testsuite}}, $testsuite;
  }
  my $xs = XML::Simple->new(NoEscape => 1);
  $xs->XMLout ($doc, RootName => 'testsuites', OutputFile => $filename)
}

sub gen_testsuite {
  my $name     = shift;
  my $time     = shift;
  my $failures = shift;
  my $skip     = shift;
  my $tests    = shift;
  my $hostname = `/bin/hostname`;

  chomp $hostname;

  return {
    name         => $name,
    hostname     => $hostname,
    errors       => 0,
    failures     => $failures,
    skip         => $skip,	    
    tests        => $tests,
    'time'       => $time,
    testcase     => [],
    timestamp    => strftime ("%Y-%m-%dT%H:%M:%S", localtime),
    'system-out' => [''],
  };
}

sub gen_testcase {
  my $name  = shift;
  my $class = shift;
  my $time  = shift;

  return {
    name    => $name,
    class   => $class,
    'time'  => $time,
    failure => [],
    skipped => [], 	    
  };
}

sub gen_failure {
  my $type = shift;
  my $message = shift;
  my $content = shift;

  return {
    type    => $type,
    message => $message,
    content => sprintf ("<![CDATA[%s]]>", $content),
  };
}
