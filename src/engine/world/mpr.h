#ifndef MPR_H_
#define MPR_H_

struct clipplanes;

namespace mpr
{
    struct CubePlanes final
    {
        const clipplanes &p;

        CubePlanes(const clipplanes &p) : p(p) {}

        /**
         * @brief Returns the value of the origin element of this object's clipplanes field.
         *
         * @return copy of this.p.o
         */
        vec center() const;

        /**
         * @brief Returns element in clipplanes vec array with largest dot product
         *
         * Calculates the dot product of n with the elements of the clipplanes
         * field p. Returns the value of the element with the largest dot product,
         * or the last element if all have the same dot product.
         *
         * @param n the parameter to dot with vec elements
         *
         * @return the values of the largest dot product element
         */
        vec supportpoint(const vec &n) const;
    };

    struct SolidCube final
    {
        vec o;
        int size;

        SolidCube(float x, float y, float z, int size) : o(x, y, z), size(size) {}
        SolidCube(const vec &o, int size) : o(o), size(size) {}
        SolidCube(const ivec &o, int size) : o(o), size(size) {}

        /**
         * @brief Returns SolidCube::o plus half of size.
         *
         * Returns value equal to elementwise sum of size/2 and o. That is, half
         * of size is added to each of o's x, y, z fields.
         *
         * @return vec containing sum of o, size/2
         */
        vec center() const;

        /**
         * @brief Returns o added to size in channels specified by passed vec.
         *
         * For fields in n that are greater than zero, adds size to that field in
         * o. The values of n are irrelevant other than whether they are greater
         * than zero.
         *
         * @return vec containing values of o conditionally summed with size
         */
        vec supportpoint(const vec &n) const;
    };

    struct Ent
    {
        const physent *ent;

        Ent(const physent *ent) : ent(ent) {}

        /**
         * @brief Returns the center of this ent, modified by the ent's height.
         *
         * Returns values containing the x and y positions of the physent inside
         * this ent, and the z position plus the eye correction. The eye
         * correction is half of the difference between the aboveeye and
         * eyeheight fields.
         *
         * @return the adjusted center position of the ent
         */
        vec center() const;
    };

    /**
     * @brief Entity Oriented Bounded Box
     *
     * The orientation of the box is set by the `orient` matrix and the size of
     * the bounding box is set by the `ent->xradius`, `ent->yradius`, and
     * `ent->aboveeye`/`ent->eyeheight`.
     *
     * The orient matrix is set by the yaw of the entity `ent`.
     */
    class EntOBB final : public Ent
    {
        public:
            /**
             * @brief Constructs this EntOBB, setting orient yaw direction field.
             *
             * Sets the yaw field to the yaw angle in the physent passed, converted
             * to radians.
             *
             * @param ent the physent associated with this EntOBB
             */
            EntOBB(const physent *ent);

            vec contactface(const vec &wn, const vec &wdir) const;

            vec supportpoint(const vec &n) const;

            float left()   const;
            float right()  const;
            float back()   const;
            float front()  const;
            float bottom() const;
            float top()    const;
        private:
            matrix3 orient;
            float supportcoord(const vec &p) const;
            float supportcoordneg(const vec &p) const;

            /**
             * @brief Returns ent sizes modified by passed ln parameter
             *
             * Returns vec(ent->xradius, ent->yradius, ent->eyeheight|-ent->aboveeye) with the signs of
             * the components set by the signs of the components of `ln`. Values passed
             * to `ln` are disregarded except for sign. Zero values of components of `ln`
             * result in a negative sign.
             *
             * @param ln the vec to get sign information from
             *
             * @return position of ent modified by signs from ln
             */
            vec localsupportpoint(const vec &ln) const;
    };

    /**
     * @brief Unoriented bounding box entity
     *
     * EntFuzzy objects have their x,y boundaries (left,right,back,front) set by
     * the radius field, and the z boundaries by the aboveeye/eyeheight
     * fields. The specific x/y radii fields are not used.
     */
    struct EntFuzzy : Ent
    {
        EntFuzzy(const physent *ent) : Ent(ent) {}

        float left()   const;
        float right()  const;
        float back()   const;
        float front()  const;
        float bottom() const;
        float top()    const;
    };

    struct EntCylinder final : EntFuzzy
    {
        EntCylinder(const physent *ent) : EntFuzzy(ent) {}

        vec contactface(const vec &n, const vec &dir) const;
        vec supportpoint(const vec &n) const;
    };

    struct EntCapsule final : EntFuzzy
    {
        EntCapsule(const physent *ent) : EntFuzzy(ent) {}

        vec supportpoint(const vec &n) const;
    };

    struct EntEllipsoid final : EntFuzzy
    {
        EntEllipsoid(const physent *ent) : EntFuzzy(ent) {}

        vec supportpoint(const vec &dir) const;
    };

    struct Model
    {
        vec o, radius;
        matrix3 orient;

        /**
         * @brief Creates a new object with specified position and orientation.
         *
         * Sets position to ent + center, with center modified by the rotation matrix
         * defined by the Euler angle parameters `yaw` `pitch` `roll`.
         *
         * @param ent base position of ent
         * @param center center offset from ent, modified by Euler angles
         * @param radius radius of entity in basis directions
         * @param yaw yaw parameter of rotation for center
         * @param pitch pitch parameter of rotation for center
         * @param roll roll parameter of rotation for center
         */
        Model(const vec &ent, const vec &center, const vec &radius, int yaw, int pitch, int roll);

        /**
         * @brief Returns the origin vector of this object.
         *
         * This is the field model.o.
         *
         * @return copy of o
         */
        vec center() const;
    };

    struct ModelOBB final : Model
    {
        ModelOBB(const vec &ent, const vec &center, const vec &radius, int yaw, int pitch, int roll) :
            Model(ent, center, radius, yaw, pitch, roll)
        {}

        vec contactface(const vec &wn, const vec &wdir) const;
        vec supportpoint(const vec &n) const;
    };


    struct ModelEllipse final : Model
    {
        ModelEllipse(const vec &ent, const vec &center, const vec &radius, int yaw, int pitch, int roll) :
            Model(ent, center, radius, yaw, pitch, roll)
        {}

        vec contactface(const vec &wn, const vec &wdir) const;
        vec supportpoint(const vec &n) const;
    };

    //templates
    constexpr float boundarytolerance = 1e-3f;

    template<class T>
    bool collide(const T &p1, const EntOBB &p2)
    {
        // v0 = center of Minkowski difference
        vec v0 = p2.center().sub(p1.center());
        if(v0.iszero())
        {
            return true;  // v0 and origin overlap ==> hit
        }
        // v1 = support in direction of origin
        vec n = vec(v0).neg();
        vec v1 = p2.supportpoint(n).sub(p1.supportpoint(vec(n).neg()));
        if(v1.dot(n) <= 0)
        {
            return false;  // origin outside v1 support plane ==> miss
        }
        // v2 = support perpendicular to plane containing origin, v0 and v1
        n.cross(v1, v0);
        if(n.iszero())
        {
            return true;   // v0, v1 and origin colinear (and origin inside v1 support plane) == > hit
        }
        vec v2 = p2.supportpoint(n).sub(p1.supportpoint(vec(n).neg()));
        if(v2.dot(n) <= 0)
        {
            return false;  // origin outside v2 support plane ==> miss
        }
        // v3 = support perpendicular to plane containing v0, v1 and v2
        n.cross(v0, v1, v2);
        // If the origin is on the - side of the plane, reverse the direction of the plane
        if(n.dot(v0) > 0)
        {
            std::swap(v1, v2);
            n.neg();
        }
        ///
        // Phase One: Find a valid portal

        for(int i = 0; i < 100; ++i)
        {
            // Obtain the next support point
            vec v3 = p2.supportpoint(n).sub(p1.supportpoint(vec(n).neg()));
            if(v3.dot(n) <= 0)
            {
                return false;  // origin outside v3 support plane ==> miss
            }
            // If origin is outside (v1,v0,v3), then portal is invalid -- eliminate v2 and find new support outside face
            vec v3xv0;
            v3xv0.cross(v3, v0);
            if(v1.dot(v3xv0) < 0)
            {
                v2 = v3;
                n.cross(v0, v1, v3);
                continue;
            }
            // If origin is outside (v3,v0,v2), then portal is invalid -- eliminate v1 and find new support outside face
            if(v2.dot(v3xv0) > 0)
            {
                v1 = v3;
                n.cross(v0, v3, v2);
                continue;
            }
            ///
            // Phase Two: Refine the portal
            for(int j = 0;; j++)
            {
                // Compute outward facing normal of the portal
                n.cross(v1, v2, v3);

                // If the origin is inside the portal, we have a hit
                if(n.dot(v1) >= 0)
                {
                    return true;
                }
                n.normalize();
                // Find the support point in the direction of the portal's normal
                vec v4 = p2.supportpoint(n).sub(p1.supportpoint(vec(n).neg()));
                // If the origin is outside the support plane or the boundary is thin enough, we have a miss
                if(v4.dot(n) <= 0 || vec(v4).sub(v3).dot(n) <= boundarytolerance || j > 100)
                {
                    return false;
                }
                // Test origin against the three planes that separate the new portal candidates: (v1,v4,v0) (v2,v4,v0) (v3,v4,v0)
                // Note:  We're taking advantage of the triple product identities here as an optimization
                //        (v1 % v4) * v0 == v1 * (v4 % v0)    > 0 if origin inside (v1, v4, v0)
                //        (v2 % v4) * v0 == v2 * (v4 % v0)    > 0 if origin inside (v2, v4, v0)
                //        (v3 % v4) * v0 == v3 * (v4 % v0)    > 0 if origin inside (v3, v4, v0)
                vec v4xv0;
                v4xv0.cross(v4, v0);
                if(v1.dot(v4xv0) > 0)
                {
                    if(v2.dot(v4xv0) > 0)
                    {
                        v1 = v4;    // Inside v1 & inside v2 ==> eliminate v1
                    }
                    else
                    {
                        v3 = v4;                   // Inside v1 & outside v2 ==> eliminate v3
                    }
                }
                else
                {
                    if(v3.dot(v4xv0) > 0)
                    {
                        v2 = v4;    // Outside v1 & inside v3 ==> eliminate v2
                    }
                    else
                    {
                        v1 = v4;                   // Outside v1 & outside v3 ==> eliminate v1
                    }
                }
            }
        }
        return false;
    }

    template<class T>
    bool collide(const EntOBB &p1, const T &p2, vec *contactnormal, vec *contactpoint1, vec *contactpoint2)
    {
        // v0 = center of Minkowski sum
        const vec v01 = p1.center(),
                  v02 = p2.center();
        vec v0 = vec(v02).sub(v01);

        // Avoid case where centers overlap -- any direction is fine in this case
        if(v0.iszero())
        {
            v0 = vec(0, 0, 1e-5f);
        }
        // v1 = support in direction of origin
        vec n = vec(v0).neg(),
            v11 = p1.supportpoint(vec(n).neg()),
            v12 = p2.supportpoint(n),
            v1 = vec(v12).sub(v11);
        if(v1.dot(n) <= 0)
        {
            if(contactnormal)
            {
                *contactnormal = n;
            }
            return false;
        }
        // v2 - support perpendicular to v1,v0
        n.cross(v1, v0);
        if(n.iszero())
        {
            n = vec(v1).sub(v0);
            n.normalize();
            if(contactnormal)
            {
                *contactnormal = n;
            }
            if(contactpoint1)
            {
                *contactpoint1 = v11;
            }
            if(contactpoint2)
            {
                *contactpoint2 = v12;
            }
            return true;
        }
        vec v21 = p1.supportpoint(vec(n).neg()),
            v22 = p2.supportpoint(n),
            v2 = vec(v22).sub(v21);
        if(v2.dot(n) <= 0)
        {
            if(contactnormal)
            {
                *contactnormal = n;
            }
            return false;
        }
        // Determine whether origin is on + or - side of plane (v1,v0,v2)
        n.cross(v0, v1, v2);
        // If the origin is on the - side of the plane, reverse the direction of the plane
        if(n.dot(v0) > 0)
        {
            std::swap(v1, v2);
            std::swap(v11, v21);
            std::swap(v12, v22);
            n.neg();
        }
        ///
        // Phase One: Identify a portal
        for(int i = 0; i < 100; ++i)
        {
            // Obtain the support point in a direction perpendicular to the existing plane
            // Note: This point is guaranteed to lie off the plane
            vec v31 = p1.supportpoint(vec(n).neg()),
                v32 = p2.supportpoint(n),
                v3 = vec(v32).sub(v31);
            if(v3.dot(n) <= 0)
            {
                if(contactnormal) *contactnormal = n;
                return false;
            }
            // If origin is outside (v1,v0,v3), then eliminate v2 and loop
            vec v3xv0;
            v3xv0.cross(v3, v0);
            if(v1.dot(v3xv0) < 0)
            {
                v2 = v3;
                v21 = v31;
                v22 = v32;
                n.cross(v0, v1, v3);
                continue;
            }
            // If origin is outside (v3,v0,v2), then eliminate v1 and loop
            if(v2.dot(v3xv0) > 0)
            {
                v1 = v3;
                v11 = v31;
                v12 = v32;
                n.cross(v0, v3, v2);
                continue;
            }
            bool hit = false;
            ///
            // Phase Two: Refine the portal
            // We are now inside of a wedge...
            for(int j = 0;; j++)
            {
                // Compute normal of the wedge face
                n.cross(v1, v2, v3);
                // Can this happen???  Can it be handled more cleanly?
                if(n.iszero())
                {
                    return true;
                }
                n.normalize();
                // If the origin is inside the wedge, we have a hit
                if(n.dot(v1) >= 0 && !hit)
                {
                    if(contactnormal)
                    {
                        *contactnormal = n;
                    }
                    // Compute the barycentric coordinates of the origin
                    if(contactpoint1 || contactpoint2)
                    {
                        float b0 = v3.scalartriple(v1, v2),
                              b1 = v0.scalartriple(v3, v2),
                              b2 = v3.scalartriple(v0, v1),
                              b3 = v0.scalartriple(v2, v1),
                              sum = b0 + b1 + b2 + b3;
                        if(sum <= 0)
                        {
                            b0 = 0;
                            b1 = n.scalartriple(v2, v3);
                            b2 = n.scalartriple(v3, v1);
                            b3 = n.scalartriple(v1, v2);
                            sum = b1 + b2 + b3;
                        }
                        if(contactpoint1)
                        {
                            *contactpoint1 = (vec(v01).mul(b0).add(vec(v11).mul(b1)).add(vec(v21).mul(b2)).add(vec(v31).mul(b3))).mul(1.0f/sum);
                        }
                        if(contactpoint2)
                        {
                            *contactpoint2 = (vec(v02).mul(b0).add(vec(v12).mul(b1)).add(vec(v22).mul(b2)).add(vec(v32).mul(b3))).mul(1.0f/sum);
                        }
                    }
                    // HIT!!!
                    hit = true;
                }
                // Find the support point in the direction of the wedge face
                const vec v41 = p1.supportpoint(vec(n).neg()),
                          v42 = p2.supportpoint(n),
                          v4 = vec(v42).sub(v41);
                // If the boundary is thin enough or the origin is outside the support plane for the newly discovered vertex, then we can terminate
                if(v4.dot(n) <= 0 || vec(v4).sub(v3).dot(n) <= boundarytolerance || j > 100)
                {
                    if(contactnormal)
                    {
                        *contactnormal = n;
                    }
                    return hit;
                }
                // Test origin against the three planes that separate the new portal candidates: (v1,v4,v0) (v2,v4,v0) (v3,v4,v0)
                // Note:  We're taking advantage of the triple product identities here as an optimization
                //        (v1 % v4) * v0 == v1 * (v4 % v0)    > 0 if origin inside (v1, v4, v0)
                //        (v2 % v4) * v0 == v2 * (v4 % v0)    > 0 if origin inside (v2, v4, v0)
                //        (v3 % v4) * v0 == v3 * (v4 % v0)    > 0 if origin inside (v3, v4, v0)
                vec v4xv0;
                v4xv0.cross(v4, v0);
                if(v1.dot(v4xv0) > 0) // Compute the tetrahedron dividing face d1 = (v4,v0,v1)
                {
                    if(v2.dot(v4xv0) > 0) // Compute the tetrahedron dividing face d2 = (v4,v0,v2)
                    {
                        // Inside d1 & inside d2 ==> eliminate v1
                        v1 = v4;
                        v11 = v41;
                        v12 = v42;
                    }
                    else
                    {
                        // Inside d1 & outside d2 ==> eliminate v3
                        v3 = v4;
                        v31 = v41;
                        v32 = v42;
                    }
                }
                else
                {
                    if(v3.dot(v4xv0) > 0) // Compute the tetrahedron dividing face d3 = (v4,v0,v3)
                    {
                        // Outside d1 & inside d3 ==> eliminate v2
                        v2 = v4;
                        v21 = v41;
                        v22 = v42;
                    }
                    else
                    {
                        // Outside d1 & outside d3 ==> eliminate v1
                        v1 = v4;
                        v11 = v41;
                        v12 = v42;
                    }
                }
            }
        }
        return false;
    }
}

#endif
