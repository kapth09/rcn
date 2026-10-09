Name: 		rcn
Version: 	0.0.2
Release:        %autorelease
Summary: 	Remote control program for linux

License: 	GPLv3
URL: 		https://github.com/kapth09/rcn
Source0: 	rcn-0.0.2.tar.gz

BuildRequires: 	gcc, make

%description
A remote control program to replay eventX inputs on another computer.


%prep
%autosetup -c


%build
%make_build


%install
mkdir -p %{buildroot}%{_bindir}
install -m 0755 rcn %{buildroot}%{_bindir}/rcn


%files
%{_bindir}/rcn
%doc README.md


%changelog
%autochangelog
