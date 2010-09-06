#!/usr/bin/perl
use warnings;
use strict;
use Test::More tests => 2;

use Cwd;
use English qw( -no_match_vars );
use File::Find;
use File::Copy;
use File::Path qw( make_path remove_tree );
use File::Spec::Functions qw( rel2abs catfile catdir );

# Works whether we are run from the parent directory or test directory.
sub test_path
{
    my ($relpath) = @_;
    $relpath = catfile( 'test', $relpath ) unless ( getcwd() =~ /test$/ );
    return rel2abs( $relpath );
}

my ( $REPOS_BUILD, $REPOS_SHARE, $TEST_ROOT, $TEST_CONF )
    = map { test_path( $_ ) } qw{ build share root test.conf };

my $start_dir = cwd();

sub create_conf
{
    my ($conf_path) = @_;

    chdir $start_dir;
    open my $conf_file, '>', $conf_path
        or die "failed to open $conf_path file: $!";

    print $conf_file <<"END_CONF";
[options]
RootDir  = $TEST_ROOT
DBPath   = $TEST_ROOT/db/
CacheDir = $TEST_ROOT/cache/
LogFile  = $TEST_ROOT/test.log

[simpletest]
Server = file:///$REPOS_SHARE/simpletest

END_CONF

    close $conf_file;
}

sub create_adder
{
    my ($repo_name) = @_;
    my $reposhare   = "$REPOS_SHARE/$repo_name";

    return sub {
        return unless /[.]pkg[.]tar[.]xz$/;
        system 'repo-add', "$reposhare/$repo_name.db.tar.gz", $File::Find::name
            and die "error ", $? >> 8, " with repo-add in $REPOS_SHARE";
        rename $_, "$reposhare/$_";
    }
}

sub create_repos
{
    local $ENV{PKGDEST} = undef;

    opendir BUILDDIR, $REPOS_BUILD
        or die "couldn't opendir on $REPOS_BUILD: $!";

    my $makepkg_opts = join q{ }, qw/ -f -d -c /,
      ( $EFFECTIVE_USER_ID == 0 ? '--asroot' : qw// );

    my $origdir = getcwd();

    chdir $REPOS_BUILD;
    my @repos = grep { !/^[.]/ && -d $_ } readdir BUILDDIR;
    closedir BUILDDIR;

    # Loop through each repository's build directory...
    for my $repodir ( @repos ) {
        my $repoabs = catdir( $REPOS_BUILD, $repodir );
        opendir REPODIR, $repoabs
            or die "couldn't opendir on $repoabs";
        chdir $repoabs
            or die qq{cannot chdir to repodir "$repodir"};

        print "$repoabs\n";

        # Create each package, which is a PKGBUILD in each subdir...
        for my $pkgdir ( grep { !/[.]{1,2}/ && -d $_ } readdir REPODIR ) {
            chdir catdir( $repoabs, $pkgdir )
                or die qq{cannot chdir to pkgdir "$pkgdir"};

            system "makepkg $makepkg_opts >/dev/null 2>&1"
                and die 'error code ', $? >> 8, ' from makepkg in $pkgdir: ';
        }
        closedir REPODIR;

        # Move each repo's package to the share dir and add it to the
        # repo's db.tar.gz file...
        my $repodest = catdir( $REPOS_SHARE, $repodir );
        make_path( $repodest, { mode => 0755 } );
        find( create_adder( $repodir ), $repoabs );
    }

    chdir $origdir;

    return @repos;
}

sub clean_root
{
    die "WTF?" if $TEST_ROOT eq '/';

    print "\$TEST_ROOT = $TEST_ROOT\n";

    remove_tree( $TEST_ROOT, { keep_root => 1 } );
    make_path( catdir( $TEST_ROOT, qw/ db local / ),
               catdir( $TEST_ROOT, qw/ db sync  / ),
               catdir( $TEST_ROOT, qw/ cache    / ),
               { mode => 0755 } );
    return 1;
}

sub corrupt_package
{
    my $fqp = rel2abs( catdir( $REPOS_SHARE,
                               qw{ simpletest
                                   corruptme-1.0-1-any.pkg.tar.xz } ));

    open my $pkg_file, '>', $fqp
        or die "failed to open file whilst corrupting: $!";
    print $pkg_file "HAHA PWNED!\n";
    close $pkg_file;

    return;
}

SKIP:
{
    skip 'test repositories are already created', 1
        if ( -e "$REPOS_SHARE" );
    diag( "creating test repositories" );
    my @repos = create_repos();
    ok( @repos, 'create test package repository' );

    @repos = map { qq{'$_'} } @repos;
    my $repos_list = join q{ and },
      ( join q{, }, @repos[0 .. $#repos-1] ), $repos[-1];

    diag( "created $repos_list repos" );

    corrupt_package();
}

# Allows me to tweak the test.conf file and not have it overwritten...
create_conf( $TEST_CONF ) unless ( -e $TEST_CONF );

diag( "initializing our test rootdir" );
ok( clean_root(), 'remake fake root dir' );

# ok( ALPM->load_config( $TEST_CONF ), 'load our generated config' );
# #ALPM->set_opt( 'logcb', sub { printf STDERR '[%10s] %s', @_; } );

# for my $reponame ( 'simpletest', 'upgradetest' ) {
#     my $repopath = sprintf( 'file://%s/%s',
# 			    rel2abs( $REPOS_SHARE ),
# 			    $reponame );
#     ok( my $db = ALPM->register_db( $reponame, $repopath ));
#     $db->update;
# }


