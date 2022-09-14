function __fish_makoctl_complete_no_subcommand
	for i in (commandline -opc)
		if contains -- $i dismiss restore invoke menu list reload help
			return 1
		end
	end
	return 0
end

complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a dismiss -d 'Dismiss notification (the last one if none is given)' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a restore -d 'Restore the most recently expired notification from the history buffer' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a invoke -d 'Invoke an action on the notification (the last one if none is given)' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a menu -d 'Use a program to select one action to be invoked on the notification (the last one if none is given)' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a list -d 'List notifications' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a history -d 'List history' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a reload -d 'Reload the configuration file' -x
complete -c makoctl -n '__fish_makoctl_complete_no_subcommand' -a help -d 'Show help message and quit' -x

complete -c makoctl -n '__fish_seen_subcommand_from dismiss' -s a -l all -d "Dismiss all notifications" -x
complete -c makoctl -n '__fish_seen_subcommand_from dismiss' -s g -l group -d "Dismiss all the notifications in the last notification's group" -x
complete -c makoctl -n '__fish_seen_subcommand_from dismiss' -s n -d "Dismiss the notification with the given id" -x
complete -c makoctl -n '__fish_seen_subcommand_from invoke' -s n -d "Invoke an action on the notification with the given id" -x
complete -c makoctl -n '__fish_seen_subcommand_from menu' -s n -d "Use a program to select one action on the notification with the given id" -x
complete -c makoctl -n '__fish_seen_subcommand_from menu' -a "(__fish_complete_command)" -x

