Summary: xfwm4, next generation window manager for xfce
Name: xfwm4
Version: 0.1.0
Release: 1
URL: http://people.redhat.com/~hp/metacity/
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: User Interface/Desktops
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gtk2-devel >= 2.0.0
BuildRequires: glib2-devel >= 2.0.0
BuildRequires: pango-devel >= 1.0.0

%description

Xfwm4 is a window manager compatable with GNOME, GNOME2, KDE2, KDE3 and Xfce.

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT mandir=%{_mandir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc example.xfwm4rc example.gtkrc-2.0 README
%{_bindir}/*
%{_datadir}/xfwm4


