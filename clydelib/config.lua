
return {
    --[[
    --clyde specific options
    --]]
['op_s_search_aur_only'] = false;
['op_s_search_repos_only'] = false;
['op_s_upgrade_aur'] = false;
['op_use_color'] = nil;
['editor'] = nil;
['op_g_get_deps'] = false;
['op_s_build_user'] = false;
    --[[
    --pacman feature functions
    --]]
['op'] = "PM_OP_MAIN";
['quiet'] = false;
['verbose'] = 0;
['version'] = false;
['help'] = false;
['confirm'] = true;
['noprogressbar'] = false;
['logmask'] = {};
--	/* unfortunately, we have to keep track of paths both here and in the library
--	 * because they can come from both the command line or config file, and we
--	 * need to ensure we get the order of preference right. */
['configfile'] = false;
['rootdir'] = false;
['dbpath'] = false;
['logfile'] = false;
--	/* TODO how to handle cachedirs? */
['op_q_isfile'] = false;
['op_q_info'] = 0;
['op_q_list'] = false;
['op_q_foreign'] = false;
['op_q_unrequired'] = false;
['op_q_deps'] = false;
['op_q_explicit'] = false;
['op_q_owns'] = false;
['op_q_search'] = false;
['op_q_changelog'] = false;
['op_q_upgrade'] = false;
['op_q_check'] = false;

['op_s_clean'] = 0;
['op_s_downloadonly'] = false;
['op_s_info'] = false;
['op_s_sync'] = 0;
['op_s_search'] = false;
['op_s_upgrade'] = 0;
['op_s_printuris'] = false;

['group'] = 0;
--	pmtransflag_t flags;
['flags'] = {};
--	/* conf file options */
['chomp'] = false; --/* I Love Candy! */
['showsize'] = false; --/* show individual package sizes */
--	/* When downloading, display the amount downloaded, rate, ETA, and percent
--	 * downloaded of the total download list */
['totaldownload'] = false;
['cleanmethod'] = "CLEAN_KEEPINST"; --/* select -Sc behavior */
['holdpkg'] = {};
--['syncfirst'] = {"pacman", "xvidcore", "xorg-server"};
['syncfirst'] = {};
['xfercommand'] = false;
['mkpkgopts'] = {}
}
