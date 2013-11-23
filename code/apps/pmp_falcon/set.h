/* 
 * Copyright (C) 2011 David Mazieres (dm@uun.org)
 *               2011 Joshua B. Leners (leners@cs.utexas.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#ifndef REP_SET_H
#define REP_SET_H 1

#include <itree.h>

/*
 * A simple set abstraction.
 */

template <class T>
struct set_entry {
    itree_entry<set_entry> link;
    T v;

    set_entry(T iv) : v(iv) {}
};

template <class T>
class set {
 private:
    itree<T, set_entry<T>, &set_entry<T>::v, &set_entry<T>::link> mem_;
    set(const set<T> &);

 public:
    set() {}
    ~set() { mem_.deleteall(); }

    int size() const {
	int count = 0;
	set_entry<T> *e = mem_.first();
	while (e) {
	    count++;
	    e = mem_.next(e);
	}
	return count;
    }

    bool contains(T v) {
	set_entry<T> *e = mem_[v];
	return (e && e->v == v);
    }

    bool insert(T v) {
	if (contains(v))
	    return false;

	mem_.insert(New set_entry<T>(v));
	return true;
    }

    void remove(T v) {
	set_entry<T> *e = mem_[v];
	if (!e || e->v != v)
	    return;

	mem_.remove(e);
	delete e;
    }

    vec<T> members() const {
	vec<T> v;
	set_entry<T> *e = mem_.first();
	while (e) {
	    v.push_back(e->v);
	    e = mem_.next(e);
	}
	return v;
    }

    void union_with(const set &s, set &r) {
	set_entry<T> *e1 = mem_.first();
	set_entry<T> *e2 = s.mem_.first();
	while (e1 && e2) {
	    if (e1->v < e2->v) {
		r->insert(e1->v);
		e1 = mem_.next(e1);
	    } else if (e2->v < e1->v) {
		r->insert(e2->v);
		e2 = s.mem_.next(e2);
	    } else {
		r->insert(e1->v);
		e1 = mem_.next(e1);
		e2 = s.mem_.next(e2);
	    }
	}

	while (e1) {
	    r->insert(e1->v);
	    e1 = mem_.next(e1);
	}

	while (e2) {
	    r->insert(e2->v);
	    e2 = s.mem_.next(e2);
	}
    }

    void intersect_with(const set &s, set *r) {
	set_entry<T> *e1 = mem_.first();
	set_entry<T> *e2 = s.mem_.first();
	while (e1 && e2) {
	    if (e1->v < e2->v) {
		e1 = mem_.next(e1);
	    } else if (e2->v < e1->v) {
		e2 = s.mem_.next(e2);
	    } else {
		r->insert(e1->v);
		e1 = mem_.next(e1);
		e2 = s.mem_.next(e2);
	    }
	}
    }

    bool contains_majority_of(const set &s) {
	set common;
	intersect_with(s, &common);
	return common.size() >= s.size() / 2 + 1;
    }

    void clear() {
	mem_.deleteall();
    }

    set &operator=(const set &s) {
	if (this != &s) {
	    mem_.deleteall();
	    set_entry<T> *e = s.mem_.first();
	    while (e) {
		insert(e->v);
		e = s.mem_.next(e);
	    }
	}
	return *this;
    }
};

#endif
