/**
 * @copyright
 * Copyright (c) 2012, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 *
 * @file TemplateVec.h
 * @date 2010-02-26
 * @author Thomas Fischer
 */

#ifndef TEMPLATEVEC_H_
#define TEMPLATEVEC_H_

#include <algorithm>

namespace GeoLib
{
/**
 * \ingroup GeoLib
 *
 * \brief The class TemplateVec takes a unique name and manages
 * a std::vector of pointers to data elements of type T.
 *
 * Instances are classes PolylineVec and SurfaceVec.
 * */
template <class T> class TemplateVec
{
protected:
	typedef std::pair<std::string, std::size_t> NameIndexType;
	typedef std::map<std::string, std::size_t> NameIndexMap;
public:
	/**
	 * Constructor of class TemlateVec.
	 * @param name unique name of the project the elements belonging to.
	 * In order to access the data elements a unique name is required.
	 * @param data_vec vector of data elements
	 * @param elem_name_map Names of data elements can be given by a
	 * std::map<std::string, std::size_t>. Here the std::string is the name
	 * of the element and the value for std::size_t stands for an index in
	 * the data_vec.

	 */
	TemplateVec (const std::string &name, std::vector<T*>* data_vec,
	             NameIndexMap* elem_name_map = nullptr) :
		_name(name), _data_vec(data_vec), _name_id_map (elem_name_map)
	{}

	/**
	 * destructor, deletes all data elements
	 */
	virtual ~TemplateVec ()
	{
		for (std::size_t k(0); k < size(); k++) delete (*_data_vec)[k];
		delete _data_vec;
		delete _name_id_map;
	}

	/** sets the name of the vector of geometric objects
	 * the data elements belonging to
	 * \param n the name as standard string */
	void setName (const std::string & n) { _name = n; }
	/**
	 * the name, the data element belonging to
	 * @return the name of the object
	 */
	std::string getName () const { return _name; }

	/**
	 * @return the number of data elements
	 */
	std::size_t size () const { return _data_vec->size(); }

	/**
	 * get a pointer to a standard vector containing the data elements
	 * @return the data elements
	 */
	const std::vector<T*>* getVector () const { return _data_vec; }

	/**
	 * search the vector of names for the ID of the geometric element with the given name
	 * @param name the name of the geometric element
	 * @param id the id of the geometric element
	 * @return
	 */
	bool getElementIDByName (const std::string& name, std::size_t &id) const
	{
		auto it (_name_id_map->find (name));

		if (it != _name_id_map->end())
		{
			id = it->second;
			return true;
		}
		else return false;
	}

	/// Returns an element with the given name.
	const T* getElementByName (const std::string& name) const
	{
		std::size_t id;
		bool ret (getElementIDByName (name, id));
		if (ret)
			return (*_data_vec)[id];
		else
			return nullptr;
	}

	/**
	 * The method returns true if there is a name associated
	 * with the given id, else method returns false.
	 * @param id the id
	 * @param element_name if a name associated with the id
	 * is found name is assigned to element_name
	 * @return if there is name associated with the given id:
	 * true, else false
	 */
	bool getNameOfElementByID (std::size_t id, std::string& element_name) const
	{
		if (!_name_id_map) return false;
		// search in map for id
		auto it(std::find_if(_name_id_map->begin(), _name_id_map->end(), PairSecondIsEqual(id)));
		if (it == _name_id_map->end()) {
			return false;
		}
		element_name = it->first;
		return true;
	}

	/// Return the name of an element based on its ID.
	void setNameOfElementByID (std::size_t id, std::string& element_name)
	{
		if (!_name_id_map) return;
		_name_id_map->insert( NameIndexType(element_name, id) );
	}

	/**
	 * The method returns true if the given element of type T
	 * can be found and the element has a name, else method returns false.
	 * @param data the data element, one wants to know the name
	 * @param name the name of the data element (if the data element is
	 * found and the data element has a name)
	 * @return if element is found and has a name: true, else false
	 */
	bool getNameOfElement (const T* data, std::string& name) const
	{
		for (std::size_t k(0); k < _data_vec->size(); k++)
			if ((*_data_vec)[k] == data)
				return getNameOfElementByID (k, name);

		return false;
	}

	/// Adds a new element to the vector.
	virtual void push_back (T* data_element, std::string const* const name = nullptr)
	{
		_data_vec->push_back (data_element);
		if (!name) return;
		if (!name->empty())
		{
			if (!_name_id_map)
				_name_id_map = new NameIndexMap;
			_name_id_map->insert (NameIndexType(*name, _data_vec->size() - 1));
		}
	}

	/// Sets the given name for the element of the given ID.
	void setNameForElement(std::size_t id, std::string const& name)
	{
		if (!_name_id_map)
			_name_id_map = new NameIndexMap;

		if ( !_name_id_map->empty())
		{
			for (auto it = _name_id_map->begin(); it != _name_id_map->end(); ++it)
				if (it->second == id)
				{
					_name_id_map->erase(it); //check if old name already exists and delete it
					break;
				}
		}
		if (!name.empty()) {
			//insert new or revised name
			_name_id_map->insert(NameIndexType(name, id));
		}
	}

protected:
	/** copy constructor doesn't have an implementation */
	// compiler does not create a (possible unwanted) copy constructor
	TemplateVec (const TemplateVec &);
	/** assignment operator doesn't have an implementation */
	// this way the compiler does not create a (possible unwanted) assignment operator
	TemplateVec& operator= (const TemplateVec& rhs);

	/** the name of the object */
	std::string _name;

	/**
	 * pointer to a vector of data elements
	 */
	std::vector <T*>* _data_vec;
	/**
	 * store names associated with the element ids
	 */
	NameIndexMap* _name_id_map;
private:
	struct PairSecondIsEqual {
		PairSecondIsEqual(std::size_t const& id) : _id (id) {}
		bool operator() (std::pair<std::string, std::size_t> const& pair)
		{
			return pair.second == _id;
		}
		std::size_t const& _id;
	}; // end struct PairSecondIsEqual
};
} // end namespace GeoLib

#endif /* TEMPLATEVEC_H_ */
